#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "amcom.h"

/// Start of packet character
const uint8_t  AMCOM_SOP         = 0xA1;
const uint16_t AMCOM_INITIAL_CRC = 0xFFFF;

static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc)
{
	byte ^= (uint8_t)(crc & 0x00ff);
	byte ^= (uint8_t)(byte << 4);
	return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
}


void AMCOM_InitReceiver(AMCOM_Receiver* receiver, AMCOM_PacketHandler packetHandlerCallback, void* userContext) {
	if ( receiver != NULL){
			receiver->receivedPacket.header.sop = 0;
			receiver->receivedPacket.header.type = 0;
			receiver->receivedPacket.header.length = 0;
			receiver->receivedPacket.header.crc = 0;
            receiver->payloadCounter = 0;
			
			receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
			receiver->packetHandler = packetHandlerCallback;
			receiver->userContext = userContext;
	}
}

size_t AMCOM_Serialize(uint8_t packetType, const void* payload, size_t payloadSize, uint8_t* destinationBuffer) {
	if ( destinationBuffer != NULL || payloadSize <= 200){

		destinationBuffer[0] = AMCOM_SOP;
		destinationBuffer[1] = packetType;
		destinationBuffer[2] = payloadSize;

		uint8_t * pay = (uint8_t *)payload;


		uint16_t crc = AMCOM_INITIAL_CRC;
		
		crc = AMCOM_UpdateCRC(packetType, crc);
		crc = AMCOM_UpdateCRC(payloadSize, crc);
		
		for(int i = 0;i<payloadSize;i++){
			crc = AMCOM_UpdateCRC(pay[i], crc);
		}
		destinationBuffer[3] = (uint8_t)(crc & 0x00ff);
		destinationBuffer[4] = (uint8_t)(crc >> 8);
		
		for(int j=5;j < payloadSize+5; j++){
			destinationBuffer[j] = pay[j-5];
		}
		
		return payloadSize+5;
	}
	return 0;
}

void AMCOM_Deserialize(AMCOM_Receiver* receiver, const void* data, size_t dataSize) {
    uint8_t temp_data_buff[dataSize];
    memcpy(&temp_data_buff, data, dataSize);
    
    for(int i = 0; i < dataSize; i++){
        
        switch(receiver->receivedPacketState){
            
            case 0:{
				//printf ("Case 0\n");
                if(temp_data_buff[i] == AMCOM_SOP){
                    receiver->receivedPacket.header.sop = AMCOM_SOP;
                    receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_SOP;
                }
                break;
            }
            
            case 1:{
				//printf ("Case 1\n");
                receiver->receivedPacket.header.type = temp_data_buff[i];
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_TYPE;
                break;
            }
            
            case 2:{
				//printf ("Case 2\n");
                if(temp_data_buff[i] > 200){
                    receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
                }
                else{
                    receiver->receivedPacket.header.length = temp_data_buff[i];
                    receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_LENGTH;
                }
                break;
            }
            
            case 3:{
                //printf ("Case 3\n");
                receiver->receivedPacket.header.crc = temp_data_buff[i];
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_CRC_LO;
                break;
            }
            
            
           case 4:{
            //printf ("Case 4\n");
                if(receiver->receivedPacket.header.length == 0){
                    uint16_t temp_crc = 0;
                    temp_crc = receiver->receivedPacket.header.crc | temp_crc;
                    temp_crc = temp_crc & 0x00ff;
                    temp_crc = temp_crc | (((uint16_t)temp_data_buff[i]) << 8);
                    receiver->receivedPacket.header.crc = temp_crc;
                    receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
                }
                else{
                    uint16_t temp_crc = 0;
                    temp_crc = receiver->receivedPacket.header.crc | temp_crc;
                    temp_crc = temp_crc & 0x00ff;
                    temp_crc = temp_crc | (((uint16_t)temp_data_buff[i]) << 8);
                    receiver->receivedPacket.header.crc = temp_crc;
                    receiver->receivedPacketState = AMCOM_PACKET_STATE_GETTING_PAYLOAD;
                }
                break;
            }
            case 6:{
                //printf ("Case 6\n");
                if(receiver->payloadCounter < receiver->receivedPacket.header.length){
                    receiver->receivedPacket.payload[receiver->payloadCounter] = temp_data_buff[i];
                    receiver->payloadCounter += 1;
                    //printf("Increasing payload counter!\n");
                }
                if ( receiver->payloadCounter == receiver->receivedPacket.header.length){
                    //printf ("Set got whole!\n");
                    receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
                }
                break;
            }
            
            default :{
                break;
            }
        }
            if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_WHOLE_PACKET){
				//printf("Got whole packet!\n");
                if(receiver->receivedPacket.header.length == 0){
                    uint16_t temp_crc = AMCOM_INITIAL_CRC;
                    temp_crc = AMCOM_UpdateCRC(receiver->receivedPacket.header.type, temp_crc);
    	            temp_crc = AMCOM_UpdateCRC(receiver->receivedPacket.header.length, temp_crc);
    	            if(receiver->receivedPacket.header.crc == temp_crc){
    	                receiver->packetHandler(&(receiver->receivedPacket), receiver->userContext);
    	                receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY; 
    	                receiver->payloadCounter = 0;
    	                
    	            }
    	            else {
    	                receiver->payloadCounter = 0;
    	                receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
    	            }
                }
                else{
                    uint16_t temp_crc = AMCOM_INITIAL_CRC;
                    temp_crc = AMCOM_UpdateCRC(receiver->receivedPacket.header.type, temp_crc);
    	            temp_crc = AMCOM_UpdateCRC(receiver->receivedPacket.header.length, temp_crc);
    	            for(int i = 0; i < receiver->receivedPacket.header.length; i++){
    	                temp_crc = AMCOM_UpdateCRC(receiver->receivedPacket.payload[i], temp_crc);
    	            }
    	            if(receiver->receivedPacket.header.crc == temp_crc){
    	                receiver->packetHandler(&(receiver->receivedPacket), receiver->userContext);
    	                receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY; 
    	                receiver->payloadCounter = 0;
    	               
    	            }
    	            else{
    	                receiver->payloadCounter = 0;
    	                receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
    	            }
                }
            }
            

    }
}
