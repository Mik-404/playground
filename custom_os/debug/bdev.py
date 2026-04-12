import sys

def examine_sectors(idx, seccount):
    with open("disk.img", "rb") as f:
        f.seek(idx * 512)
        print(f.read(seccount * 512))

examine_sectors(int(sys.argv[1]), int(sys.argv[2]))
