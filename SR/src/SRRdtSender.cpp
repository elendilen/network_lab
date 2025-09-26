#include "SRRdtSender.h"
#include "Global.h"
#include <cstring>
#include <iostream>
using namespace std;

SRRdtSender::SRRdtSender()
    : expectSequenceNumberSend(0), base(0), winSize(4), seqLen(8), waitingState(false)
{
}

SRRdtSender::~SRRdtSender() {}

/**
 * @brief 检查发送方是否处于等待状态（窗口满）
 */
bool SRRdtSender::getWaitingState() {
    int usedWindow = (expectSequenceNumberSend - base + seqLen) % seqLen;
    return usedWindow >= winSize;
}

/**
 * @brief 发送应用层消息
 */
bool SRRdtSender::send(const Message& message) {
    if (getWaitingState()) {
        return false; // 窗口满，不能发送
    }

    // 构造报文
    Packet pkt;
    pkt.seqnum = expectSequenceNumberSend;
    pkt.acknum = -1;
    pkt.checksum = 0;
    memcpy(pkt.payload, message.data, sizeof(message.data));
    pkt.checksum = pUtils->calculateCheckSum(pkt);

    // 加入发送窗口
    sendWindow[pkt.seqnum] = pkt;

    // 发送并启动独立定时器
    pUtils->printPacket("发送方发送报文", pkt);
    pns->sendToNetworkLayer(RECEIVER, pkt);
    pns->stopTimer(SENDER, pkt.seqnum);
    pns->startTimer(SENDER, Configuration::TIME_OUT, pkt.seqnum);

    expectSequenceNumberSend = (expectSequenceNumberSend + 1) % seqLen;
    return true;
}

/**
 * @brief 处理收到的ACK
 */
void SRRdtSender::receive(const Packet& ackPkt) {
    int checkSum = pUtils->calculateCheckSum(ackPkt);
    if (checkSum != ackPkt.checksum) {
        pUtils->printPacket("发送方收到错误ACK", ackPkt);
        return;
    }

    int offset = (ackPkt.acknum - base + seqLen) % seqLen;
    if (offset >= winSize || sendWindow.find(ackPkt.acknum) == sendWindow.end()) {
        pUtils->printPacket("发送方收到重复或窗口外ACK", ackPkt);
        return;
    }

    // 停止定时器并删除对应包
    pns->stopTimer(SENDER, ackPkt.acknum);
    sendWindow.erase(ackPkt.acknum);
    pUtils->printPacket("发送方收到确认ACK", ackPkt);

    // 更新 base 指向窗口最左未确认包
    while (!sendWindow.empty() && sendWindow.find(base) == sendWindow.end()) {
        base = (base + 1) % seqLen;
    }

    // 窗口空时 base 指向下一个待发送序号
    if (sendWindow.empty()) {
        base = expectSequenceNumberSend;
    }
}

/**
 * @brief 超时处理
 */
void SRRdtSender::timeoutHandler(int seqNum) {
    auto it = sendWindow.find(seqNum);
    if (it != sendWindow.end()) {
        pUtils->printPacket("发送方超时重传", it->second);
        pns->stopTimer(SENDER, seqNum);
        pns->startTimer(SENDER, Configuration::TIME_OUT, seqNum);
        pns->sendToNetworkLayer(RECEIVER, it->second);
    }
}
