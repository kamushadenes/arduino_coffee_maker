import requests
import sys

class CoffeeMakerAPI(object):

    def __init__(self, hostname, port):
        self.hostname = hostname
        self.port = port

    def request(self, method):
        r = requests.request(method, 'http://{}:{}'.format(self.hostname, self.port))
        return r


if __name__ == '__main__':
    cma = CoffeeMakerAPI(sys.argv[1], sys.argv[2])

    print(cma.request(sys.argv[3]).content)
