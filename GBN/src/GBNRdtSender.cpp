#include "Global.h"
#include "GBNRdtSender.h"
#include <deque>


GBNRdtSender::GBNRdtSender() :expectSequenceNumberSend(0), base(0), winSize(4), seqLen(8), waitingState(false) { //窗口大小 = 4，序号长度二进制位数 = 3
}


GBNRdtSender::~GBNRdtSender() {
}


bool GBNRdtSender::getWaitingState() {
	if (packetWindow.size() == winSize) { //如果发送方窗口满，则处于等待确认状态
		this->waitingState = true;
	}
	else {
		this->waitingState = false;
	}
	return this->waitingState;
}


bool GBNRdtSender::send(const Message& message) {
	if (this->getWaitingState() == true) { //发送窗口满，不能发送应用层来的message
		return false;
	}

	this->packetWaitingAck.acknum = -1; //忽略该字段
	this->packetWaitingAck.seqnum = this->expectSequenceNumberSend;
	this->packetWaitingAck.checksum = 0;
	memcpy(this->packetWaitingAck.payload, message.data, sizeof(message.data));
	this->packetWaitingAck.checksum = pUtils->calculateCheckSum(this->packetWaitingAck);

	packetWindow.push_back(packetWaitingAck); //将待发送的包加入发送方窗口队列尾部
	pUtils->printPacket("发送方发送报文", this->packetWaitingAck);

	if (this->base == this->expectSequenceNumberSend) { //如果发送序号等于base，则启动发送方定时器
		pns->startTimer(SENDER, Configuration::TIME_OUT, this->base);
	}

	pns->sendToNetworkLayer(RECEIVER, this->packetWaitingAck); //调用模拟网络环境的sendToNetworkLayer，通过网络层发送到对方

	this->expectSequenceNumberSend = (this->expectSequenceNumberSend + 1) % this->seqLen; //更新下一个待发送包的序号，在0~7之间

	return true;
}


void GBNRdtSender::receive(const Packet& ackPkt) {
	int checkSum = pUtils->calculateCheckSum(ackPkt);
	int offPosition = (ackPkt.acknum - this->base + this->seqLen) % this->seqLen;

	if (checkSum == ackPkt.checksum && offPosition < packetWindow.size()) { //判断校验和是否正确以及acknum是否在报文段队列中
		std::cout << "发送方窗口: [";
		for (int i = 0; i < this->winSize - 1; i++) {
			cout << (this->base + i) % this->seqLen << ", ";
		}
		std::cout << (this->base + this->winSize - 1) % this->seqLen << "]" << std::endl;
		pUtils->printPacket("发送方正确收到确认", ackPkt);
		pns->stopTimer(SENDER, this->base); //关闭base计时器

		while (this->base != (ackPkt.acknum + 1) % this->seqLen) { //滑动窗口到acknum+1位置
			packetWindow.pop_front(); //移除发送方窗口队列头部报文段
			this->base = (this->base + 1) % this->seqLen;
		}
		cout << "发送方窗口: [";
		for (int i = 0; i < this->winSize - 1; i++) {
			std::cout << (this->base + i) % this->seqLen << ", ";
		}
		std::cout << (this->base + this->winSize - 1) % this->seqLen << "]" << std::endl;

		if (packetWindow.size() != 0) { //如果发送方窗口队列非空，继续以当前base为准，重新开启计时器
			pns->startTimer(SENDER, Configuration::TIME_OUT, this->base);
		}
	}
	else if (checkSum != ackPkt.checksum) { //校验和不正确
		pUtils->printPacket("发送方没有正确收到确认", ackPkt);
	}
	else { //该报文段的ACK之前已经正确接收
		pUtils->printPacket("发送方已经确认过该报文段被正确接收", ackPkt);
	}
}


void GBNRdtSender::timeoutHandler(int seqNum) {
	pns->stopTimer(SENDER, seqNum);										//首先关闭定时器
	pns->startTimer(SENDER, Configuration::TIME_OUT, seqNum);			//重新启动发送方定时器

	for (int i = 0; i < packetWindow.size(); i++) { //重新发送窗口内所有的报文段
		pUtils->printPacket("超时，GBN重发窗口所有报文", packetWindow.at(i));
		pns->sendToNetworkLayer(RECEIVER, packetWindow.at(i));
	}
}

