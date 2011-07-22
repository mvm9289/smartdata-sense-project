'''
Created on 25/08/2010
rev 26/11/2010
@author: Zolertia
lan
'''
import serial 
import binascii
import threading
import sys
from optparse import OptionParser
import datetime
from xml.dom.minidom import *
import ftplib
from math import sqrt

#from smtplib import SMTP_SSL as SMTP       # this invokes the secure SMTP protocol (port 465, uses SSL)
# from smtplib import SMTP                  # use this for standard SMTP protocol   (port 25, no encryption)
from email.MIMEText import MIMEText



class MyZ1HarvesterData(threading.Thread):
    '''
    Example of packet:
    id mota: 01 08 
    kind: 03 
    size: 14 00 
    counter: 00 00 
    data: 00 D8 01 DA 02 DA 03 DB 04 DB 05 D7 06 DA 07 DB 08 DC 09 DA 58 99 9B 37 E2 30 00 00 B9 37 D6 30 
    Reserved: 34 00 00 00 00 00 25 04 88 00 06 11 01 00 1C A7 1E 00 E0 30 00 00 94 
    check sum: 17 00

    '''
    
    #0.DEFINE YOUR CONSTANTS
    BAUDRATE=115200
    TEMPN = 1800 #a xml must be send every 300 sec (sync with temperature packet)
    MOTEISOUTAT= 1
    #if a mote in our list of registered motes didn't send any 
                    #packet after 15 min .... his status must be 0
    
    #common name for our packets
    DESC_PACKETKIND = "packet_kind"
    DESC_MOTEIDENTIFICATION = "mote_identification"
    DESC_PACKETSIZE="packet_size"
    
    
    #descriptors for data that you want send. Their name and number depends of the packets
    #that the mote must process
    DESC_PACKETNUMBER ="packet_number"
    DESC_DATETIME="date_and_time"
    
    DESC_GENERIC_1 ="HelloBaseStation"
    DESC_GENERICVALUE_1="HelloBaseStation_value"
    DESC_GENERICRESERVED_1="HelloBaseStation_reserved"
    DESC_CHECKSUM_1="HelloBaseStation_checksum"
    
    DESC_GENERIC_2 ="HelloMobileNode"
    DESC_GENERICVALUE_2="HelloMobileNode_value"
    DESC_GENERICRESERVED_2="HelloMobileNode_reserved"
    DESC_CHECKSUM_2="HelloMobileNode_checksum"
    
    DESC_GENERIC_3 ="Datos"
    DESC_GENERICVALUE_3="Datos_value"
    DESC_GENERICRESERVED_3="Datos_reserved"
    DESC_CHECKSUM_3="Datos_checksum"

    DESC_GENERIC_4 ="Ack"
    DESC_GENERICVALUE_4="Ack_value"
    DESC_GENERICRESERVED_4="Ack_reserved"
    DESC_CHECKSUM_4="Ack_checksum"

    DESC_GENERIC_5 ="Poll"
    DESC_GENERICVALUE_5="Poll_value"
    DESC_GENERICRESERVED_5="Poll_reserved"
    DESC_CHECKSUM_5="Poll_checksum"    
           
    def __init__(self, portName):
        
        #zolertia demo
        self.__dateTime = [] 
        self.__temperature = []


                
        '''
        Constructor
        '''
        self.__packetsTemplates =[] #an array of packets
        self.__motes =[] #an array of motes
        self.__currentPacketReceived = None #current packet read from serial port
        self.__currentPacketIndexTemplate = -1 #the kind template of current packet 
        self.__dataMustBeSend = 0 #if true the data must be send 
	
            
        #1.PACKET/S DESCRIPTION: THAT IS A PACKET TEMPLATE AND DATA CONTAINER 
        self.definePacketsTemplates()
                
        #2.OPEN SERIAL PORT     
        try:
            self.__serialPort = self.openSerialPort(portName)
        except serial.SerialException:
            sys.stderr.write("Error at open port: " + portName)
            sys.exit(1)  
            
        #3.AND NOW WAIT FOR PACKETS    
        threading.Thread.__init__ ( self )
        if self.__serialPort.isOpen():
            self.__waitforpackets= 1
            self.start()  #start run ......           
    
    def definePacketsTemplates(self):
        
        self.__packetsTemplates.append(PacketDescription(self.DESC_GENERIC_1,1))
        self.__packetsTemplates[0].setDescriptionContentAtIndex(0, [self.DESC_MOTEIDENTIFICATION,2,[]])
        self.__packetsTemplates[0].setDescriptionContentAtIndex(2, [self.DESC_PACKETKIND,1,[]])
        self.__packetsTemplates[0].setDescriptionContentAtIndex(3, [self.DESC_PACKETSIZE,2,[]])
        self.__packetsTemplates[0].setDescriptionContentAtIndex(5, [self.DESC_PACKETNUMBER,2,[]])
        self.__packetsTemplates[0].setDescriptionContentAtIndex(7, [self.DESC_GENERICVALUE_1,32,[]])
        self.__packetsTemplates[0].setDescriptionContentAtIndex(39, [self.DESC_GENERICRESERVED_1,23,[]])
        self.__packetsTemplates[0].setDescriptionContentAtIndex(62, [self.DESC_CHECKSUM_1,2,[]])
        self.__packetsTemplates[0].getDescriptionContent().append([self.DESC_DATETIME,0,[]]) 
    
        self.__listOfGeneric_1_Content = [self.DESC_MOTEIDENTIFICATION,self.DESC_PACKETKIND, self.DESC_PACKETSIZE, 
                                                 self.DESC_PACKETNUMBER,self.DESC_GENERICVALUE_1,self.DESC_GENERICRESERVED_1,
                                                 self.DESC_CHECKSUM_1,self.DESC_DATETIME]
        
        self.__packetsTemplates.append(PacketDescription(self.DESC_GENERIC_2,2))
        self.__packetsTemplates[1].setDescriptionContentAtIndex(0, [self.DESC_MOTEIDENTIFICATION,2,[]])
        self.__packetsTemplates[1].setDescriptionContentAtIndex(2, [self.DESC_PACKETKIND,1,[]])
        self.__packetsTemplates[1].setDescriptionContentAtIndex(3, [self.DESC_PACKETSIZE,2,[]])
        self.__packetsTemplates[1].setDescriptionContentAtIndex(5, [self.DESC_PACKETNUMBER,2,[]])
        self.__packetsTemplates[1].setDescriptionContentAtIndex(7, [self.DESC_GENERICVALUE_2,32,[]])
        self.__packetsTemplates[1].setDescriptionContentAtIndex(39, [self.DESC_GENERICRESERVED_2,23,[]])
        self.__packetsTemplates[1].setDescriptionContentAtIndex(62, [self.DESC_CHECKSUM_2,2,[]])
        self.__packetsTemplates[1].getDescriptionContent().append([self.DESC_DATETIME,0,[]]) 
    
        self.__listOfGeneric_2_Content = [self.DESC_MOTEIDENTIFICATION,self.DESC_PACKETKIND, self.DESC_PACKETSIZE, 
                                                 self.DESC_PACKETNUMBER,self.DESC_GENERICVALUE_2,self.DESC_GENERICRESERVED_2,
                                                 self.DESC_CHECKSUM_2,self.DESC_DATETIME]  
        
        self.__packetsTemplates.append(PacketDescription(self.DESC_GENERIC_3,3))
        self.__packetsTemplates[2].setDescriptionContentAtIndex(0, [self.DESC_MOTEIDENTIFICATION,2,[]])
        self.__packetsTemplates[2].setDescriptionContentAtIndex(2, [self.DESC_PACKETKIND,1,[]])
        self.__packetsTemplates[2].setDescriptionContentAtIndex(3, [self.DESC_PACKETSIZE,2,[]])
        self.__packetsTemplates[2].setDescriptionContentAtIndex(5, [self.DESC_PACKETNUMBER,2,[]])
        self.__packetsTemplates[2].setDescriptionContentAtIndex(7, [self.DESC_GENERICVALUE_3,32,[]])
        self.__packetsTemplates[2].setDescriptionContentAtIndex(39, [self.DESC_GENERICRESERVED_3,23,[]])
        self.__packetsTemplates[2].setDescriptionContentAtIndex(62, [self.DESC_CHECKSUM_3,2,[]])
        self.__packetsTemplates[2].getDescriptionContent().append([self.DESC_DATETIME,0,[]]) 
    
        self.__listOfGeneric_3_Content = [self.DESC_MOTEIDENTIFICATION,self.DESC_PACKETKIND, self.DESC_PACKETSIZE, 
                                                 self.DESC_PACKETNUMBER,self.DESC_GENERICVALUE_3,self.DESC_GENERICRESERVED_3,
                                                 self.DESC_CHECKSUM_3,self.DESC_DATETIME]               

        self.__packetsTemplates.append(PacketDescription(self.DESC_GENERIC_4,4))
        self.__packetsTemplates[3].setDescriptionContentAtIndex(0, [self.DESC_MOTEIDENTIFICATION,2,[]])
        self.__packetsTemplates[3].setDescriptionContentAtIndex(2, [self.DESC_PACKETKIND,1,[]])
        self.__packetsTemplates[3].setDescriptionContentAtIndex(3, [self.DESC_PACKETSIZE,2,[]])
        self.__packetsTemplates[3].setDescriptionContentAtIndex(5, [self.DESC_PACKETNUMBER,2,[]])
        self.__packetsTemplates[3].setDescriptionContentAtIndex(7, [self.DESC_GENERICVALUE_4,32,[]])
        self.__packetsTemplates[3].setDescriptionContentAtIndex(39, [self.DESC_GENERICRESERVED_4,23,[]])
        self.__packetsTemplates[3].setDescriptionContentAtIndex(62, [self.DESC_CHECKSUM_4,2,[]])
        self.__packetsTemplates[3].getDescriptionContent().append([self.DESC_DATETIME,0,[]]) 
    
        self.__listOfGeneric_4_Content = [self.DESC_MOTEIDENTIFICATION,self.DESC_PACKETKIND, self.DESC_PACKETSIZE, 
                                                 self.DESC_PACKETNUMBER,self.DESC_GENERICVALUE_4,self.DESC_GENERICRESERVED_4,
                                                 self.DESC_CHECKSUM_4,self.DESC_DATETIME] 
        
        self.__packetsTemplates.append(PacketDescription(self.DESC_GENERIC_5,5))
        self.__packetsTemplates[4].setDescriptionContentAtIndex(0, [self.DESC_MOTEIDENTIFICATION,2,[]])
        self.__packetsTemplates[4].setDescriptionContentAtIndex(2, [self.DESC_PACKETKIND,1,[]])
        self.__packetsTemplates[4].setDescriptionContentAtIndex(3, [self.DESC_PACKETSIZE,2,[]])
        self.__packetsTemplates[4].setDescriptionContentAtIndex(5, [self.DESC_PACKETNUMBER,2,[]])
        self.__packetsTemplates[4].setDescriptionContentAtIndex(7, [self.DESC_GENERICVALUE_5,32,[]])
        self.__packetsTemplates[4].setDescriptionContentAtIndex(39, [self.DESC_GENERICRESERVED_5,23,[]])
        self.__packetsTemplates[4].setDescriptionContentAtIndex(62, [self.DESC_CHECKSUM_5,2,[]])
        self.__packetsTemplates[4].getDescriptionContent().append([self.DESC_DATETIME,0,[]]) 
    
        self.__listOfGeneric_5_Content = [self.DESC_MOTEIDENTIFICATION,self.DESC_PACKETKIND, self.DESC_PACKETSIZE, 
                                                 self.DESC_PACKETNUMBER,self.DESC_GENERICVALUE_5,self.DESC_GENERICRESERVED_5,
                                                 self.DESC_CHECKSUM_5,self.DESC_DATETIME]              
        
    def openSerialPort(self, portName):
        return serial.Serial(port=portName,baudrate=self.BAUDRATE,parity=serial.PARITY_NONE,
                             stopbits=serial.STOPBITS_ONE,bytesize=serial.EIGHTBITS)
        
    def run ( self ):  
        while(self.__waitforpackets):
            n=self.__serialPort.inWaiting()
            if n > 0:
                #print n
                self.__currentPacketReceived = self.__serialPort.read(n)
                
                if self.isAProcesablePacket():
                    '''
                    hex = []
                    
                    stringToBeSave=""
                    
                    for aChar in self.__currentPacketReceived:
                        hex.append( "%02X " % ord( aChar ) )
                
                    print ''.join( hex ).strip()
                    ''' 
                    print self.__serialPort
                    #self.sendDataToTxtFile(stringToBeSave)  
                    #self.actualizeMoteList()
                    self.procesCurrentPacketData(self.__packetsTemplates[self.__currentPacketIndexTemplate]) 
    
                 
    #that is: is the packet received in our list of packets template
    def isAProcesablePacket(self):
        find = 0
        self.__currentPacketIndexTemplate = -1
        for item in self.__packetsTemplates:
            self.__currentPacketIndexTemplate +=1
            index_nBytes = self.getIndexAndBytesToProcess(self.DESC_PACKETKIND)
            packetKind = self.procesUnsignedData(index_nBytes)
            
            if int(packetKind) == int(self.__packetsTemplates[self.__currentPacketIndexTemplate].getPacketIdentification()):
                find = 1
                break
                                        
        if find==0: self.__currentPacketIndexTemplate = -1   
        
        return find    
    
    #that is: is the source mote in our list of motes
    def actualizeMoteList(self):
        index_nBytes = self.getIndexAndBytesToProcess(self.DESC_MOTEIDENTIFICATION)
        mote_id = self.procesUnsignedData(index_nBytes)
        position = -1
       
        try:
            if len(self.__motes)>0:
                for item in self.__motes:
                    if item.getMoteIdentification()==mote_id:
                        position=self.__motes.index(item)
        except Exception:
            pass
       
        if position ==-1: #the mote is not registred
            #add a new mote
            self.__motes.append(Z1mote(mote_id))
        else:
            #actualize last packet received timer ...
            self.__motes[position].setLastDateSend()
        '''     
            if not self.__motes[position].getMoteStatus():
                self.__motes[position].setMoteStatus(0)
 	
        for item in self.__motes:
            difference =datetime.datetime.now()-item.getLastDateSend()
            #print item.getMoteIdentification(), ": " , difference.seconds
            if difference.seconds/60 > self.MOTEISOUTAT:
                item.setMoteStatus(0)
       '''         
                
          

       
 
    def procesCurrentPacketData(self,classPacket):
        
        dataString = ""  
        kind = None   
        content = None
        templateName = classPacket.getPacketName()
        template = classPacket.getDescriptionContent()
        
        
        #get all necessary information: index, bytes to proccess and description
        for item in range(len(template)):
            content = template[item] #in content we have [description, bytes, data]
            
            if content != None:
                #if templateName==self.DESC_GENERIC_3:
                for item_aux in range(len(self.__listOfGeneric_3_Content)):
                    if str(content[0])==str(self.__listOfGeneric_3_Content[item_aux]):
                        index_nBytes =(self.getIndexAndBytesToProcess(str(content[0])))
                        if item_aux ==0: #1
                            #content[2]=[]
                            #content[2].append( str(self.procesUnsignedData(index_nBytes)))
                            dataString = str(self.procesUnsignedData(index_nBytes))
                        elif item_aux ==1: #1
                            content[2]=[]
                            kind =self.procesUnsignedData(index_nBytes)
                            
                            if kind==1:
                                #content[2].append(self.DESC_GENERIC_1)  
                                dataString = dataString + "\t" + self.DESC_GENERIC_1
                            elif kind==2:
                                #content[2].append(self.DESC_GENERIC_2) 
                                dataString = dataString + "\t" + self.DESC_GENERIC_2 
                            elif kind==3:
                                #content[2].append(self.DESC_GENERIC_3)
                                dataString = dataString + "\t" + self.DESC_GENERIC_3  
                            elif kind==4:
                                #content[2].append(self.DESC_GENERIC_4)
                                dataString = dataString + "\t" + self.DESC_GENERIC_4  
                            elif kind==5:
                                #content[2].append(self.DESC_GENERIC_5)  
                                dataString = dataString + "\t" + self.DESC_GENERIC_5
                            
                            
                        elif item_aux==2:
                            #content[2]=[]
                            #content[2].append(str(self.procesUnsignedData(index_nBytes)))
                            dataString = dataString + "\t" + str(self.procesUnsignedDataR(index_nBytes))
                        elif item_aux== 3:
                            #content[2]=[]
                            #content[2].append(str(self.procesUnsignedData(index_nBytes))) 
                            dataString = dataString + "\t" + str(self.procesUnsignedData(index_nBytes)) 
                        elif item_aux == 4:
                            #content[2]=[2]
                            #content[2].append(self.procesDataAsString(index_nBytes))
                            dataString = dataString + "\t" + self.procesDataAsString(index_nBytes)                                                                       
                        elif item_aux==7:
                            #content[2]=[]
                            #content[2].append(datetime.datetime.now())   
                            dataString = dataString + "\t" + str(datetime.datetime.now())                                 
                        #print content[2][0]
        
        self.sendDataToTxtFile(dataString,kind) 
        self.iniDataList()     
  
    #gets the position in the packet and the bytes needed to data process                                 
    def getIndexAndBytesToProcess(self, queryGroupOfBytes):
        index = self.__packetsTemplates[self.__currentPacketIndexTemplate].getIndexOfDescriptionContent(queryGroupOfBytes)
        if index > -1:
            return [index ,self.__packetsTemplates[self.__currentPacketIndexTemplate].getDescriptionContentAtIndex(index)]
        else: return[-1,None] 
   
    #proces one or more bytes of unsigned data    
    def procesUnsignedData(self, index_nBytes):
        n =int((index_nBytes[1])[1])
        ini=int(index_nBytes[0])
        
        if n == 1:
            return int(self.__currentPacketReceived[ini].encode("hex"),16)
        else:
            i = 0
            sum =self.__currentPacketReceived[ini + i].encode("hex")
            i +=1
            while i<n:
                sum += self.__currentPacketReceived[ini + i].encode("hex")
                i += 1
            return int(sum,16)
 
    def procesUnsignedDataR(self, index_nBytes):
        n =int((index_nBytes[1])[1])
        n = n*(-1)
        ini=int(index_nBytes[0])
        
        if n == 1:
            return int(self.__currentPacketReceived[ini].encode("hex"),16)
        else:
            i = 1
            sum =self.__currentPacketReceived[ini + i].encode("hex")
            i -=1
            while i>n:
                sum += self.__currentPacketReceived[ini + i].encode("hex")
                i -= 1
            return int(sum,16)
    #ini the list of proces data
    def iniDataList(self):     
        for item in self.__packetsTemplates:    
            for item_aux in item.getDescriptionContent():
                if item_aux != None:
                    item_aux[2]=[]

                                                        
    def procesAccelerationData(self, index_nBytes, valueKind):
        sum= self.procesUnsignedData(index_nBytes)
        if sum > 32767:
            sum =float(sum-(1<<16))
            
        axe = self.__listOfAccelerationPacketContent[valueKind]
         
        if axe == self.DESC_ZAXEVALUE:
            sum = sum / 230
        else:
            sum = sum / 256             
        
        return sum
  
    def procesTemperatureData(self, index_nBytes):
        sum= self.procesUnsignedData(index_nBytes)
        if sum >2047 :
            sum = float(sum-(1<<12))
                      
        return float(sum * 0.0625) 

    def procesDataAsString(self, index_nBytes):
        n =int((index_nBytes[1])[1])
        ini=int(index_nBytes[0])
        total = n + ini

        hex = []
        stringToBeSave=""
        count = ini
        
        while count < total:
            aChar = self.__currentPacketReceived[count]
            hex.append( "%02X " % ord( aChar ) )
            count = count +1 
            
        '''           
        for aChar in self.__currentPacketReceived:
            if count >= ini & count < total:
                print count
                hex.append( "%02X " % ord( aChar ) )
                
            count = count +1  
        '''      
    
        #print ''.join( hex ).strip()
        stringToBeSave=''.join(hex).strip() 

        return stringToBeSave  
 

    def sendDataToTxtFile(self, stringtowrite, kind):
        try:
            import os.path
            
            titols="Id Mota \t Tipo Mensaje \t Size \t Contador \t Datos \t Date-Time"
            
            if kind==3:
                if os.path.exists("./DataReceived.txt"):
                    session = file( "./DataReceived.txt", 'a' ) #a append data
                else:
                    session = file( "./DataReceived.txt", 'a' ) 
                    session.write(titols + "\n")  
            else:
                if os.path.exists("./noDataKindReceived.txt"):
                    session = file( "./noDataKindReceived.txt", 'a' ) #a append data
                else:
                    session = file( "./noDataKindReceived.txt", 'a' ) 
                    session.write(titols + "\n")                 
                    
            session.write(stringtowrite + "\n")
            session.close()
           
            

        except Exception:
            print "! error: " + str(sys.exc_info()[1])
           
        finally:
            pass    
 
 
              
class Z1mote:
    
    def __init__(self, moteId):
        '''
        Constructor
        '''
        self.__moteIdentification = moteId #mote identificator
        self.__moteStatus=1  #is mote live or not
        #last time that this mote has send a packet
        #or last time that the sink has received a packet ....       
        self.__lastDateSend=datetime.datetime.now()  
   
    def getMoteIdentification(self):
        return self.__moteIdentification
    
    def getMoteStatus(self):
        return self.__moteStatus
 
    def setMoteStatus(self, status):
        self.__moteStatus=status
        
    def getLastDateSend(self):   
        return self.__lastDateSend
  
    def setLastDateSend(self):
        self.__lastDateSend=datetime.datetime.now()
        
class PacketDescription:
    
    PACKETMAXLEN = 46 # 46 max length (TinyOS 2.x standard) 
    '''
    Constructor
    '''
    def __init__(self, packetname="", packetId=-1):
        self.__packetName=packetname #a string name for the packet
        self.__packetIdentification=packetId #a numeric name for the packet, that is the id insert on the packet
        '''
        byteContent is a list of PACKETMAXLEN elements. The position of each element is the 
        same that their initial position in the packet send by a source node. Each element is composed of a
        list with a literal description of the information contained in a byte/s, the number of bytes occupied
        by each data and a list of extracted and processed data packets received.
        '''       
        self.__byteContent=[]
        
        i= 0
        while i< self.PACKETMAXLEN:
            self.__byteContent.append(None)
            i +=1
           
        self.__intervalTime = 1000 #default value 1000ms
        

    def getPacketName(self):
        return self.__packetName
 
    def getPacketIdentification(self):
        return self.__packetIdentification
       
    def getDescriptionContent(self): #return all
        return self.__byteContent
  
    def getDescriptionContentAtIndex(self, index): #return the content at xx list position)
        try:
            value =self.__byteContent[index]
        except Exception:
            value = "Index out of range"
        return value    
 
    def getIndexOfDescriptionContent(self, description): #return the position of the first 'description' ocurrence 
        i = 0
        find = 0
        for item in self.__byteContent:
            if item !=None:
                if str(description) == str(item[0]):
                    find=1
                    break
            i+=1
        
        if find==1:return i 
        else:return -1  
       
    def setDescriptionContentAtIndex(self, index, value):   
        try:
            self.__byteContent[index]=value
        except Exception:
            return "Index  out of range"
        

    def setIntervalTime(self, value): 
        try:
            self.__intervalTime=int(value)
        except Exception:
            return "The interval time must be an integer"
  
    def getIntervalTime(self):
        return self.__intervalTime  
    
   
def main():
  
    usage = "usage: %prog [options] arg"
    parser = OptionParser(usage)

    (options, args) = parser.parse_args()
    if len(args) != 1:
        parser.error("Micorsoft Windows: write 'COMxx' or Linux SO: write '/dev/ttyUSBxx'." +
                     " Verify the serial port number. If the serial port number is correct"  +
                     " and you are Linux user try to change the serial port permissions:  " +
                     "sudo chmod 666 /dev/ttyUSBxx")
    else:
        MyZ1HarvesterData(args[0])
     

if __name__ == "__main__":
    main()     
