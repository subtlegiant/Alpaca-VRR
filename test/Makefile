all: send rcv

rcv: recvmsg.c
	gcc recvmsg.c -o rcv

send: sendmsg.c
	gcc sendmsg.c -o send

clean:
	rm rcv send
