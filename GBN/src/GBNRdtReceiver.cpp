#include "Global.h"
#include "GBNRdtReceiver.h"


GBNRdtReceiver::GBNRdtReceiver() :expectSequenceNumberRcvd(0), seqLen(8) { //接收方窗口长度为1，序号长度二进制位数为3
	lastAckPkt.acknum = -1; //初始状态下，上次发送的确认包的确认序号为-1，使得当第一个接受的数据包出错时该确认报文的确认号为-1
	lastAckPkt.checksum = 0;
	lastAckPkt.seqnum = -1;	//忽略该字段
	for (int i = 0; i < Configuration::PAYLOAD_SIZE; i++) {
		lastAckPkt.payload[i] = '.';
	}
	lastAckPkt.checksum = pUtils->calculateCheckSum(lastAckPkt);
}


GBNRdtReceiver::~GBNRdtReceiver() {
}

void GBNRdtReceiver::receive(const Packet& packet) {
	int checkSum = pUtils->calculateCheckSum(packet);
	bool flag = false;

	if (checkSum == packet.checksum && this->expectSequenceNumberRcvd == packet.seqnum) { //检查校验和以及序号是否是接收方期待的
		pUtils->printPacket("接收方正确收到发送方的报文", packet);
		flag = true;

		Message msg;
		memcpy(msg.data, packet.payload, sizeof(packet.payload));
		pns->delivertoAppLayer(RECEIVER, msg); //接收方窗口为1，收到包就将msg递交给上层应用

		lastAckPkt.acknum = packet.seqnum; //确认报文的ack等于收到报文的seqnum
		lastAckPkt.checksum = pUtils->calculateCheckSum(lastAckPkt);
		pUtils->printPacket("接收方发送确认报文", lastAckPkt);
		pns->sendToNetworkLayer(SENDER, lastAckPkt); //调用模拟网络环境的sendToNetworkLayer，通过网络层发送确认报文到对方

		this->expectSequenceNumberRcvd = (this->expectSequenceNumberRcvd + 1) % this->seqLen; //接收序号在0~7之间切换
	}
	else if (checkSum != packet.checksum) {
		pUtils->printPacket("报文损失", packet);
	}
	else {
		pUtils->printPacket("不是接收方期待的报文", packet);
	}
	if (flag == false) {
		pUtils->printPacket("接收方重新发送上次的确认报文", lastAckPkt);
		pns->sendToNetworkLayer(SENDER, lastAckPkt); //调用模拟网络环境的sendToNetworkLayer，通过网络层发送上次的确认报文
	}
}