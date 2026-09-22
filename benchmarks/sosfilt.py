import numpy as np
from scipy.signal import butter, sosfilt


def main():
    fs = 1e3
    fcut = 100
    order = 8
    sos = butter(order, fcut, fs=fs, btype="low", output="sos")
    # print(sos)

    # Print with curly braces so I can copy/paste for C++
    for section in sos:
        print("{" + ", ".join(map(str, section)) + "}")
    x = np.random.randn(1000)
    y = sosfilt(sos, x)


if __name__ == "__main__":
    main()
