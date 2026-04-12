#include "arch/x86/x86.h"
#include "drivers/ata.h"
#include "fs/block_device.h"
#include "drivers/ioapic.h"
#include "drivers/pci.h"
#include "kernel/irq.h"
#include "kernel/panic.h"
#include "kernel/printk.h"
#include "kernel/sched.h"
#include "lib/common.h"
#include "lib/list.h"
#include "mm/obj_alloc.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"

#define ATA_ERR_AMNF     (1 << 0)
#define ATA_ERR_TKZNF    (1 << 1)
#define ATA_ERR_ABRT     (1 << 2)
#define ATA_ERR_MCR      (1 << 3)
#define ATA_ERR_IDNF     (1 << 4)
#define ATA_ERR_MC       (1 << 5)
#define ATA_ERR_UNC      (1 << 6)
#define ATA_ERR_BBK	     (1 << 7)

#define ATA_STATUS_ERR   (1 << 0)
#define ATA_STATUS_DRQ   (1 << 3)
#define ATA_STATUS_BSY   (1 << 7)

#define ATA_CMD_IDENTIFY  0xec
#define ATA_CMD_READ_DMA  0xc8
#define ATA_CMD_WRITE_DMA 0xca

#define ATA_SELECT_MASTER 0xa0

#define ATA_SECTOR_SIZE 512

#define BMR_START (1 << 0)
#define BMR_READ  (1 << 3)

namespace ata {

struct AtaBlockDevice : public BlockDevice {
private:
    ListHead<BlockDevice::BaseRequest, &BlockDevice::BaseRequest::list> req_queue_;
    pci::PrdtEntry* prdt_ = nullptr;
    uint16_t io_base_;
    uint16_t io_ctrl_base_;
    uint16_t bar4_;

private:
    enum AtaReg : uint16_t {
        ATA_REG_DATA = 0,
        ATA_REG_ERR = 1,
        ATA_REG_FEAT = 1,
        ATA_REG_SECCOUNT = 2,
        ATA_REG_LBA_LO = 3,
        ATA_REG_LBA_MID = 4,
        ATA_REG_LBA_HIGH = 5,
        ATA_REG_DRIVE = 6,
        ATA_REG_COMMAND = 7,
        ATA_REG_STATUS = 7,
    };

    enum BmrReg : uint16_t {
        BMR_REG_COMMAND = 0,
        BMR_REG_STATUS = 2,
        BMR_REG_PRDT = 4,
    };

    inline uint16_t BmrRegPort(BmrReg reg) noexcept {
        return bar4_ + reg;
    }

    inline uint16_t ControlRegPort() noexcept {
        return io_ctrl_base_;
    }

    inline uint16_t AltStatusRegPort() noexcept {
        return io_ctrl_base_;
    }

    inline uint16_t RegPort(AtaReg reg) noexcept {
        return io_base_ + reg;
    }

    inline uint16_t BmrCommandReg() noexcept {
        return bar4_;
    }

    void Wait() noexcept {
        x86::Inb(AltStatusRegPort());
        x86::Inb(AltStatusRegPort());
        x86::Inb(AltStatusRegPort());
        x86::Inb(AltStatusRegPort());
    }

    int Poll() noexcept {
        uint8_t drq = 0;
        uint8_t err = 0;
        uint8_t bsy = 1;
        while (bsy || (!drq && !err)) {
            Wait();
            uint8_t status = x86::Inb(AltStatusRegPort());
            drq = status & ATA_STATUS_DRQ;
            err = status & ATA_STATUS_ERR;
            bsy = status & ATA_STATUS_BSY;
        }
        if (err) {
            return x86::Inb(RegPort(ATA_REG_ERR));
        }
        return 0;
    }

    void IssueDmaCommand(BlockDevice::BaseRequest& req) noexcept {
        prdt_[0].buf_addr = (uint32_t)(uintptr_t)VIRT_TO_PHYS(req.buf.data);
        prdt_[0].byte_count = req.buf.size;
        prdt_[0].mark = pci::PRDT_MARK_END;

        int lba = req.sector;
        int cmd = req.flags.Has(BlockDevice::RequestFlag::Write) ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA;

        // Setup bus mastering registers.
        x86::Outb(BmrRegPort(BMR_REG_COMMAND), 0);
        x86::Outl(BmrRegPort(BMR_REG_PRDT), (uintptr_t)VIRT_TO_PHYS(prdt_));

        // Setup ATA registers.
        x86::Outb(RegPort(ATA_REG_DRIVE), ATA_SELECT_MASTER | 0x40 | ((lba >> 24) & 0x0f));
        x86::Outb(RegPort(ATA_REG_FEAT), 0);
        x86::Outb(RegPort(ATA_REG_SECCOUNT), req.buf.size / ATA_SECTOR_SIZE);
        x86::Outb(RegPort(ATA_REG_LBA_LO), lba & 0xff);
        x86::Outb(RegPort(ATA_REG_LBA_MID), (lba >> 8) & 0xff);
        x86::Outb(RegPort(ATA_REG_LBA_HIGH), (lba >> 16) & 0xff);
        x86::Outb(RegPort(ATA_REG_COMMAND), cmd);

        if (req.flags.Has(BlockDevice::RequestFlag::Write)) {
            x86::Outb(BmrRegPort(BMR_REG_COMMAND), BMR_START);
        } else {
            x86::Outb(BmrRegPort(BMR_REG_COMMAND), BMR_START | BMR_READ);
        }
    }

public:
    void IrqHandler() noexcept {
        RawScopeLocker locker(lock_);

        BUG_ON(req_queue_.Empty());

        BlockDevice::RequestPtr req(&req_queue_.First());

        // Stop DMA transfer.
        x86::Outb(BmrRegPort(BMR_REG_COMMAND), 0);

        uint8_t status = x86::Inb(AltStatusRegPort());
        int err = 0;
        if (status & ATA_STATUS_ERR) {
            err = x86::Inb(RegPort(ATA_REG_ERR));
            req->flags |= BlockDevice::RequestFlag::Error;
        }

        req->list.Remove();
        req->Unref();

        if (!req_queue_.Empty()) {
            IssueDmaCommand(req_queue_.First());
        }

        locker.Unlock();

        if (req->flags.Has(BlockDevice::RequestFlag::Error)) {
            printk("[ide] ata error %d lba=%lu\n", err, req->sector);
        }

        EndRequest(std::move(req));
        return;
    }

    kern::Errno Submit(BlockDevice::RequestPtr req) noexcept override {
        printk("[ata] %s request to lba=%lu seccount=%lu\n", req->flags.Has(BlockDevice::RequestFlag::Write) ? "WRITE" : "READ", req->sector, req->buf.size / ATA_SECTOR_SIZE);

        IrqSafeScopeLocker locker(lock_);
        bool should_issue_cmd = req_queue_.Empty();
        req->Ref();
        req_queue_.InsertLast(*req);
        if (should_issue_cmd) {
            IssueDmaCommand(*req);
        }
        return kern::ENOERR;
    }

    kern::Errno Init(pci::Device& pciDev) noexcept {
        sector_size_ = ATA_SECTOR_SIZE;

        // Select master.
        x86::Outb(RegPort(ATA_REG_DRIVE), ATA_SELECT_MASTER);
        x86::Outb(ControlRegPort(), 1 << 1);
        x86::Outb(RegPort(ATA_REG_SECCOUNT), 0);
        x86::Outb(RegPort(ATA_REG_LBA_LO), 0);
        x86::Outb(RegPort(ATA_REG_LBA_MID), 0);
        x86::Outb(RegPort(ATA_REG_LBA_HIGH), 0);
        x86::Outb(RegPort(ATA_REG_COMMAND), ATA_CMD_IDENTIFY);
        if (x86::Inb(AltStatusRegPort()) == 0) {
            printk("[ata] no ATA controller found\n");
            return kern::ENOENT;
        }

        uint8_t lba_lo = x86::Inb(RegPort(ATA_REG_LBA_LO));
        uint8_t lba_hi = x86::Inb(RegPort(ATA_REG_LBA_HIGH));
        if (lba_lo != 0 || lba_hi != 0) {
            printk("cannot init ATA primary master, lba_lo = %d, lba_hi = %d\n", lba_lo, lba_hi);
            return kern::EINVAL;
        }

        uint8_t err = Poll();
        if (err) {
            printk("[ide] ata error while initilizing device: %d", err);
            return kern::EIO;
        }

        for (int i = 0; i < 256; i++) {
            x86::Inw(RegPort(ATA_REG_DATA));
        }
        x86::Outb(ControlRegPort(), 0);

        bar4_ = pciDev.ReadBar4() & 0xfffffffc;
        BUG_ON(bar4_ == 0);

        prdt_ = (pci::PrdtEntry*)mm::AllocPageSimple(1);
        if (!prdt_) {
            printk("[ide] cannot allocate memory for PRDT\n");
            return kern::ENOMEM;
        }

        return kern::ENOERR;
    }

    AtaBlockDevice(uint16_t io_base, uint16_t io_ctrl_base)
        : io_base_(io_base)
        , io_ctrl_base_(io_ctrl_base)
    {}
};

static AtaBlockDevice primary_master(0x1f0, 0x3f6);

static void Ide1Handler(unsigned int irq) {
    UNUSED(irq);
    primary_master.IrqHandler();
}

static void Ide2Handler(unsigned int irq) {
    UNUSED(irq);
}

void Init() {
    pci::Device pciDev;
    if (!pci::FindDevice(pci::PCI_CLASS_MASS_STORAGE_CONTROLLER, pci::PCI_SUBCLASS_IDE, &pciDev)) {
        printk("[ide] no IDE controller found\n");
        return;
    }

    printk("[ide] found IDE controller, PCI bus=0x%x, dev=0x%x, func=0x%d, id=0x%x\n", pciDev.bus_, pciDev.dev_, pciDev.func_);
    pciDev.EnableBusMastering();
    kern::Errno err = primary_master.Init(pciDev);
    if (!err.Ok()) {
        printk("[ide] cannot init master: %e", err.Code());
        return;
    }

    ioapic::Enable(14, 41);
    ioapic::Enable(15, 42);

    kern::IrqAssign(41, Ide1Handler);
    kern::IrqAssign(42, Ide2Handler);

    if (auto err = BlockDevice::Register(primary_master, "ata"); !err.Ok()) {
        panic("cannot register ata block device: %e\n", err.Code());
    }
}

}
