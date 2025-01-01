from dspsim import cutil

def main():
    print(cutil.ErrorCodes._member_map_)
    for k in cutil.ErrorCodes:
        print(k.value)

    x = cutil.ErrorCodes(0).name
    print(x)

if __name__ == "__main__":
    main()