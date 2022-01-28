import os
from subprocess import Popen, PIPE, call

# os.system("./tritontalk -r 1 -s 1")
# os.system("msg 0 0 yeet")
# os.system("msg 0 0 yeet")

cmd = "./tritontalk -r 1 -s 1"

def simple_test():
    p = Popen([cmd, "msg 0 0 yeet"], stderr=PIPE, stdin=PIPE, shell=True, encoding='utf8')
    # for i in range(10):
    #     p.stdin.write("msg 0 0 PACKET: " + str(i))
    print(p.communicate())
simple_test()


