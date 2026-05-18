from segment import Segment


# #################################################################################################################### #
# RDTLayer                                                                                                             #
#                                                                                                                      #
# Description:                                                                                                         #
# The reliable data transfer (RDT) layer is used as a communication layer to resolve issues over an unreliable         #
# channel.                                                                                                             #
#                                                                                                                      #
#                                                                                                                      #
# Notes:                                                                                                               #
# This file is meant to be changed.                                                                                    #
#                                                                                                                      #
#                                                                                                                      #
# #################################################################################################################### #


class RDTLayer(object):
    # ################################################################################################################ #
    # Class Scope Variables                                                                                            #
    #                                                                                                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    DATA_LENGTH = 4 # in characters                     # The length of the string data that will be sent per packet...
    FLOW_CONTROL_WIN_SIZE = 15 # in characters          # Receive window size for flow-control
    sendChannel = None
    receiveChannel = None
    dataToSend = ''
    currentIteration = 0                                # Use this for segment 'timeouts'
    TIMEOUT_ITERATIONS = 5

    # ################################################################################################################ #
    # __init__()                                                                                                       #
    #                                                                                                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def __init__(self):
        self.sendChannel = None
        self.receiveChannel = None
        self.dataToSend = ''
        self.currentIteration = 0

        self.send_Buffer = {}
        self.send_Base = 0
        self.next_SeqNum = 0
        self.countSegmentTimeouts = 0
        self.receive_Buffer = {}
        self.expected_SeqNum = 0
        self.data_Received = ''




    # ################################################################################################################ #
    # setSendChannel()                                                                                                 #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # Called by main to set the unreliable sending lower-layer channel                                                 #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def setSendChannel(self, channel):
        self.sendChannel = channel

    # ################################################################################################################ #
    # setReceiveChannel()                                                                                              #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # Called by main to set the unreliable receiving lower-layer channel                                               #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def setReceiveChannel(self, channel):
        self.receiveChannel = channel

    # ################################################################################################################ #
    # setDataToSend()                                                                                                  #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # Called by main to set the string data to send                                                                    #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def setDataToSend(self,data):
        self.dataToSend = data

    # ################################################################################################################ #
    # getDataReceived()                                                                                                #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # Called by main to get the currently received and buffered string data, in order                                  #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def getDataReceived(self):
        # ############################################################################################################ #
        # Identify the data that has been received..
        # ############################################################################################################ #
        return self.data_Received

    # ################################################################################################################ #
    # processData()                                                                                                    #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # "timeslice". Called by main once per iteration                                                                   #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def processData(self):
        self.currentIteration += 1
        self.processSend()
        self.processReceiveAndSendRespond()

    # ################################################################################################################ #
    # processSend()                                                                                                    #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # Manages Segment sending tasks                                                                                    #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def processSend(self):
        segmentSend = Segment()
        for seqnum in list(self.send_Buffer):
            segment, timestamp, originalData = self.send_Buffer[seqnum]
        
            if (self.currentIteration - timestamp) >= RDTLayer.TIMEOUT_ITERATIONS:
                self.countSegmentTimeouts += 1
                print(f"Timeout! Retransmitting segment with seqnum: {seqnum}")

                freshSegment = Segment()
                freshSegment.setData(segment.seqnum, originalData)

                self.send_Buffer[seqnum] = (freshSegment, self.currentIteration, originalData)
    
                print(f"Sending segment: {freshSegment.to_string()}")
                self.sendChannel.send(freshSegment)
        while self.next_SeqNum < len(self.dataToSend):
            windowSize = self.next_SeqNum - self.send_Base
            if windowSize >= RDTLayer.FLOW_CONTROL_WIN_SIZE:
                break

            seqnum = self.next_SeqNum
            endPos = min(self.next_SeqNum + RDTLayer.DATA_LENGTH, len(self.dataToSend))
            data = self.dataToSend[seqnum:endPos]
        
            segmentSend = Segment()
            segmentSend.setData(seqnum, data)
        
            self.send_Buffer[seqnum] = (segmentSend, self.currentIteration, data)
        
            print(f"Sending segment: {segmentSend.to_string()}")
            self.sendChannel.send(segmentSend)
        
            self.next_SeqNum = endPos

    

    # ################################################################################################################ #
    # processReceive()                                                                                                 #
    #                                                                                                                  #
    # Description:                                                                                                     #
    # Manages Segment receive tasks                                                                                    #
    #                                                                                                                  #
    #                                                                                                                  #
    # ################################################################################################################ #
    def processReceiveAndSendRespond(self):
        segmentAck = Segment()                  # Segment acknowledging packet(s) received

        # This call returns a list of incoming segments (see Segment class)...
        listIncomingSegments = self.receiveChannel.receive()

        # ############################################################################################################ #
        # What segments have been received?
        # How will you get them back in order?
        # This is where a majority of your logic will be implemented
        for segment in listIncomingSegments:
            if segment.acknum == -1:
                if not segment.checkChecksum():
                    print(f"Checksum error! Dropping segment seqnum: {segment.seqnum}")
                    continue
                seqnum = segment.seqnum
                data = segment.payload

                if seqnum not in self.receive_Buffer:
                    self.receive_Buffer[seqnum] = data

                while self.expected_SeqNum in self.receive_Buffer:
                    data = self.receive_Buffer[self.expected_SeqNum]
                    self.data_Received += data
                    del self.receive_Buffer[self.expected_SeqNum]
                    self.expected_SeqNum += len(data)
                
                segmentAck = Segment()
                segmentAck.setAck(self.expected_SeqNum)
                print(f"Sending ack: {segmentAck.to_string()}")
                self.sendChannel.send(segmentAck)

            else:
                acknum = segment.acknum
    
                segmentsToRemove = []
                for seqnum in list(self.send_Buffer):
                    if seqnum < acknum:
                        segmentsToRemove.append(seqnum)
    
                for seqnum in segmentsToRemove:
                    del self.send_Buffer[seqnum]
    
                if acknum > self.send_Base:
                    self.send_Base = acknum