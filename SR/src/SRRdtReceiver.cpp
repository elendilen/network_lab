#include "SRRdtReceiver.h"
#include <iostream>
#include <Global.h>
SRRdtReceiver::SRRdtReceiver(): base(0), winSize(4), seqLen(8)
{
    lastAckPkt.acknum = -1;
    lastAckPkt.checksum = 0;
    lastAckPkt.seqnum = -1;
    for (int i = 0; i < Configuration::PAYLOAD_SIZE; i++) {
        lastAckPkt.payload[i] = '.';
    }
    lastAckPkt.checksum = pUtils->calculateCheckSum(lastAckPkt);
}

SRRdtReceiver::~SRRdtReceiver() {}

void SRRdtReceiver::receive(const Packet& packet)
{
    std::cout << "base: " << base << std::endl;
    
    // 1. 校验包
    int checkSum = pUtils->calculateCheckSum(packet);
    if (checkSum != packet.checksum) {
        pUtils->printPacket("接收方收到校验错误报文，丢弃", packet);
        // 校验错误包直接丢弃，不发送任何 ACK
        return;
    }

    int seqNum = packet.seqnum;
    
    // 计算偏移量：seqNum 距离 base 的“顺时针”距离
    int offset = (seqNum - base + seqLen) % seqLen;

    // 2. 窗口范围判断：分为 **旧报文** 和 **新/未来报文**

    // 2A. 窗口外 (offset >= winSize) 且是 **旧报文**
    // 旧报文的特征：seqNum 位于 [base - winSize, base - 1] 范围内
    // 检查：seqNum 到 base 的“逆时针”距离是否小于等于 winSize
    int reverse_offset = (base - seqNum + seqLen) % seqLen;

    if (offset >= winSize && reverse_offset > 0 && reverse_offset <= winSize) {
        // 这是已交付的旧报文（例如：base=1, seqNum=0, reverse_offset=1）
        pUtils->printPacket("接收方收到已交付旧报文，重发ACK", packet);
        
        // 构造并重发这个报文的 ACK
        Packet ackPkt;
        // 使用 lastAckPkt 作为模板，保证其他字段有值，但主要是 acknum
        ackPkt = lastAckPkt; 
        ackPkt.acknum = seqNum; 
        ackPkt.checksum = pUtils->calculateCheckSum(ackPkt);
        pns->sendToNetworkLayer(SENDER, ackPkt);
        return;
    } 
    // 2B. 窗口外 (offset >= winSize) 且是 **新/未来报文**
    else if (offset >= winSize) {
        pUtils->printPacket("接收方收到窗口外报文（未来报文），丢弃", packet);
        // 窗口外未来报文直接丢弃，不发送 ACK
        return;
    }
    
    // 至此，报文在窗口内 (0 <= offset < winSize)

    // 3. 处理窗口内报文 (新报文或窗口内重复报文)
    
    // 窗口内重复报文 (已缓存的报文)
    if (recvWindow.count(seqNum)) {
        pUtils->printPacket("接收方收到重复报文，重发ACK", packet);
        
        // 构造 ACK 并发送
        Packet ackPkt = lastAckPkt; 
        ackPkt.acknum = seqNum;
        ackPkt.checksum = pUtils->calculateCheckSum(ackPkt);
        pns->sendToNetworkLayer(SENDER, ackPkt);
        return;
    }
    
    // 窗口内新报文：缓存报文
    recvWindow[seqNum] = packet;
    pUtils->printPacket("接收方缓存报文", packet);

    // 4. 发送 ACK (只确认当前报文)
    // 注意：SR 协议应发送确认单个报文的 ACK，而不是依赖 lastAckPkt
    Packet currentAckPkt = lastAckPkt; 
    currentAckPkt.acknum = seqNum;
	currentAckPkt.checksum = pUtils->calculateCheckSum(currentAckPkt);
	pUtils->printPacket("接收方发送确认报文", currentAckPkt);
	pns->sendToNetworkLayer(SENDER, currentAckPkt); 
    
    // 5. 按序交付应用层
    while (recvWindow.count(base)) { // 使用 count() 检查是否存在更简洁
        Packet deliverPkt = recvWindow[base];
        Message msg;
        memcpy(msg.data, deliverPkt.payload, sizeof(deliverPkt.payload));
        pns->delivertoAppLayer(RECEIVER, msg);
        recvWindow.erase(base);
        base = (base + 1) % seqLen; // 滑动窗口
    }
}
