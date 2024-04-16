
import sys

def main():
    with open(sys.argv[2], "w") as out:
        with open(sys.argv[1], "rb") as f:
            f_bytes = f.read()
        out.write("uint64_t " + sys.argv[3] + "_len = " + str(len(f_bytes)) + "ULL;\n")
        out.write("uint8_t " + sys.argv[3] + "[] = {")
        for b in f_bytes:
            out.write(f"{b},")
        out.write("};\n");

if __name__ == "__main__":
    main()
