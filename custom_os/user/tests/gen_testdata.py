def main():
    with open("gentestdata/big_letters.txt", "w") as f:
        for i in range(26):
            print(chr(64 + i) * 5000, file=f, end='')

main()
