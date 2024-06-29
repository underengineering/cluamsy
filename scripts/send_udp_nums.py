import socket
import argparse
import time
from sys import argv, exit

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ncat num sender')
    parser.add_argument('host', type=str)
    parser.add_argument('port', type=int)
    parser.add_argument('-s', '--sleep', default=100, type=int, help='sleep time', required=False)
    parser.add_argument('--nosleep', help='nosleep', action='store_true')
    parser.add_argument('--tcp', help='use tcp instead of udp', action='store_true')
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    counter = 0
    while True: # send till die
        counter += 1
        if counter > 16:
            counter = 0

        data = "-" * counter + "\n"
        sock.sendto(data.encode(), (args.host, args.port))
        print(data, end="")

        if not args.nosleep:
            time.sleep(args.sleep / 1000.0)

