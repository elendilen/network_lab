#ifndef SR_RDT_RECEIVER_H
#define SR_RDT_RECEIVER_H
#include "RdtReceiver.h"
#include <map>
class SRRdtReceiver :public RdtReceiver
{
private:
	int base;	//期待收到的下一个报文序号
	int seqLen;                     //接收序号长度
    int winSize;
    std::map<int,Packet> recvWindow;
    Packet lastAckPkt;

public:
	SRRdtReceiver();
	virtual ~SRRdtReceiver();

public:
	void receive(const Packet& packet);	//接收报文，将被NetworkService调用
};

#endif