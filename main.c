#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "amcom.h"
#include "amcom_packets.h"

#define PI 3.14f
#define MAX_NUMBER_OF_TRANSISTORS 65535

int number_of_transistors = 0;

enum {
    MAX_NUMBER_OF_PLAYERS = 8, //from docs
    AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE = 11, //from docs
    FOOD_UPDATE_REQUEST_SINGLE_SIZE = 11 //from docs
};

int player_number = 0;
int init_number_of_players = 0;
float map_width = 0;
float map_height = 0;

uint8_t decision = 0; //0 - food, more than 1 - player

float eat_player_angle = 1.0f;
float eat_food_angle = 2.0f;

float distance_to_food = 2000.0f;
float closest_food_x = 100.0f;
float closest_food_y = 100.0f;
float closest_player = 0.0f;
float closest_player_x = 0.0f;
float closest_player_y = 0.0f;

float decided_angle = 0.0f;

float this_x = 0.0f;
float this_y = 0.0f;

int eat = 0;

int chosenTransistor = 512;

unsigned short perf1=1;
AMCOM_PlayerState playerState[MAX_NUMBER_OF_PLAYERS]; //better waste memory but keep it static than use malloc
static AMCOM_PlayerState* thisPlayer; /*to avoid storing twice the information concerning our player and to use the least memory possible we will have a pointer
                                to an element of the array above using the index assigned to the player - that is player_number*/

AMCOM_FoodState transistorState[MAX_NUMBER_OF_TRANSISTORS];

// float f_abs(float input)
// {
//     return sqrt(input*input);
// }

float calculate_distance (float x_0, float y_0, float x_1, float y_1)
{   
    //printf("X_0:%f x_1:%f\n",x_0,x_1);
    float x_2 = (x_0-x_1);
    x_2 *= x_2;
    float y_2 = (y_0-y_1);
    y_2 *= y_2;
    float sum = x_2+y_2;
    
    return sqrt(sum);
}

// int get_closest_player_id()
// {
//     int closest_id = 0;
//     float closest = 1000.0f;
//     for (int i = 0; i < init_number_of_players; i++)
//     {
//         if (i != player_number)
//         {
//             float dist = calculate_distance (playerState[i].x, playerState[i].y, thisPlayer->x, thisPlayer->y);
//             if (dist < closest)
//             {
//                 closest = dist;
//                 closest_id = i;
//             }
//         }        
//     }
//     closest_player = closest;
//     closest_player_x = playerState[closest_id].x;
//     closest_player_y = playerState[closest_id].y;
//     //printf("Distance: %f\n", closest);
//     //printf ("Closest x, y: %f, %f\n", closest_player_x, closest_player_y);
//     return closest_id;
// }

// void decide ()
// {
//     uint8_t points = 0;
//     int id = get_closest_player_id();
//     if (playerState[id].hp < thisPlayer->hp)
//     {
//         //attack??
//         points++;
//     }
//     if (closest_player < distance_to_food)
//     {
//         if (points)
//         {
//             points++;
//         } else {
//             points--;
//         }
//     }
//     decision = points;
// }

int do_every_10_times = 0;

void amPacketHandler(const AMCOM_Packet* packet, void* userContext) {
    uint8_t buf[AMCOM_MAX_PACKET_SIZE];
    size_t toSend = 0;

    switch (packet->header.type) {
    case AMCOM_IDENTIFY_REQUEST:
    {
        //printf("Got IDENTIFY.request. Responding with IDENTIFY.response\n");
        AMCOM_IdentifyResponsePayload identifyResponse;
        sprintf(identifyResponse.playerName, "wosiolak");
        toSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), buf);
        break;
    }
    case AMCOM_NEW_GAME_REQUEST:
    {
        AMCOM_NewGameRequestPayload newGameRequest;
        newGameRequest.playerNumber = packet->payload[0]; // player number 0...7
        newGameRequest.numberOfPlayers = packet->payload[1]; // all players number in game 
        u_char b0[] = {packet->payload[2], packet->payload[3], packet->payload[4], packet->payload[5]}; // map_width - little endian 
        memcpy(&map_width, &b0, sizeof(map_width)); 
        printf ("Map width: %f", map_width);
        u_char b1[] = {packet->payload[6], packet->payload[7], packet->payload[8], packet->payload[9]}; // map_height - little endian 
        memcpy(&map_height, &b1, sizeof(map_height));
        printf ("Map width: %f", map_height);
        init_number_of_players = newGameRequest.numberOfPlayers;
        printf("Got NEW_GAME.request. Player number: %d, number of players: %d\n", newGameRequest.playerNumber, newGameRequest.numberOfPlayers);
        player_number = newGameRequest.playerNumber;
        thisPlayer = &playerState[player_number];
        AMCOM_NewGameResponsePayload newGameResponse;
        sprintf(newGameResponse.helloMessage, "Hello wosiolak");
        toSend = AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, &newGameResponse, sizeof(newGameResponse),buf);

        break;
    }

    case AMCOM_PLAYER_UPDATE_REQUEST:
    {   
        //printf("Got player update request\n");
        for (int i = 0; i < init_number_of_players; i++)
        {
            playerState[i].playerNo=packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+0];
            //printf("Got data about player: %d", playerState[i].playerNo);
            u_char b0[] = {packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+1], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+2]}; // Health points - little endian 
            memcpy(&(playerState[i].hp), &b0, sizeof(playerState[i].hp));

            u_char b1[] = {packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+3], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+4], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+5], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+6]}; // X axis player position - little endian
            memcpy(&(playerState[i].x), &b1, sizeof(playerState[i].x));

            u_char b2[] = {packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+7], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+8], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+9], packet->payload[AMCOM_PLAYER_UPDATE_REQUEST_SINGLE_SIZE*i+10]}; // Y axis player position - little endian
            memcpy(&(playerState[i].y), &b2, sizeof(playerState[i].y));
            //printf("INIT X%f Y%f PLAYER\n",thisPlayer->x,thisPlayer->y);


        }
        
        // The packet should not be responded to
       
    }
    
    case AMCOM_FOOD_UPDATE_REQUEST:
    {
        //printf("Food update request\n");
        //printf("In food update: X%f Y%f PLAYER\n",thisPlayer->x,thisPlayer->y);
        
        
        for (int i = 0; i < packet->header.length/FOOD_UPDATE_REQUEST_SINGLE_SIZE; i++) // dlugosc payloadu/11
        {
            //printf ("enter looop\n");
            AMCOM_FoodState foodState;
            u_char b0[] = {packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+0], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+1]}; // food number - little endian 
            memcpy(&(foodState.foodNo), &b0, sizeof(foodState.foodNo));

            u_char b1[] = {packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+2]}; // food state - little endian 
            memcpy(&(foodState.state), &b1, sizeof(foodState.state));
            //printf ("Number of transistor: %d\n", foodState.foodNo);
            //printf ("State: %d\n", foodState.state);
            // if (0 == foodState.state)
            // {
            //     continue;   // zjedzone - > nie inicjalizuj pozycji 
            // }

            if (1 == foodState.state)
            {
                u_char b2[] = {packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+3], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+4], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+5], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+6]}; // X axis food position - little endian
                memcpy(&(foodState.x), &b2, sizeof(foodState.x));

                u_char b3[] = {packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+7], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+8], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+9], packet->payload[FOOD_UPDATE_REQUEST_SINGLE_SIZE*i+10]}; // Y axis food position - little endian
                memcpy(&(foodState.y), &b3, sizeof(foodState.y)); 

                transistorState[number_of_transistors] = foodState;

                number_of_transistors++;
            } else {
                if (foodState.foodNo <= number_of_transistors)
                {
                    transistorState[foodState.foodNo].state = 0;
                    printf ("Eaten transistor no: %d\n", foodState.foodNo);
                }
            }

             
            
        }   

         // The packet should not be responded to
       
    }

    case AMCOM_MOVE_REQUEST:
    {
        if (eat == 0){
            int ch = 0;
        //printf("Got move request\n");
        distance_to_food = 2000.0f;
            for (int j = 0; j < number_of_transistors; j++)
            {
                if (1 == transistorState[j].state)
                {
                    float dist = calculate_distance (transistorState[j].x, transistorState[j].y, thisPlayer->x, thisPlayer->y); // pozycja naszego gracza która nie działa #fixme 
                    if (dist < distance_to_food)
                    {
                        distance_to_food = dist;
                        closest_food_x = transistorState[j].x;
                        closest_food_y = transistorState[j].y;
                        ch = j;
                    }
                }
            }

        if (ch!= chosenTransistor)
        {
            chosenTransistor = ch;
            decided_angle =(atan2( (closest_food_y-thisPlayer->y),(closest_food_x-thisPlayer->x)))+PI;
            printf("Chosen: %d\n", ch);
            printf ("Set angle: %f\n", decided_angle);
        }       
        eat++;
        } else {
            if (eat == 1)
            {
                eat = 0;
            }
        }

        //printf ("Distance = %f\n", distance_to_food);

        //printf("wosiolak X:%f Y:%f \n", thisPlayer->x , thisPlayer->y);
        //printf("zarcie: X=%f, Y=%f\n", closest_food_x, closest_food_y);
        //printf("ANGLE IN radjany:%f\n",decided_angle);
        
        AMCOM_MoveRequestPayload moveRequestPayload;
        u_char b0[] = {packet->payload[0], packet->payload[1],packet->payload[2],packet->payload[3]}; // game time - little endian 
        memcpy(&(moveRequestPayload.gameTime), &b0, sizeof(moveRequestPayload.gameTime));

        AMCOM_MoveResponsePayload moveResponsePayload;
        //scanf("%f", &(moveResponsePayload.angle)); // insert angle in radians from console 
        moveResponsePayload.angle = decided_angle;
        toSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponsePayload, sizeof(moveResponsePayload), buf);
    }

    case AMCOM_GAME_OVER_REQUEST:
    {

    }



    }
    

    // retrieve socket from user context
    SOCKET ConnectSocket  = *((SOCKET*)userContext);
    // send response if any
    int bytesSent = send(ConnectSocket, (const char*)buf, toSend, 0 );
    if (bytesSent == SOCKET_ERROR) {
        printf("Socket send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return;
    }
}


#define GAME_SERVER "localhost"
#define GAME_SERVER_PORT "2001"

int main(int argc, char **argv) {
    printf("This is mniAM player. Let's eat some transistors! \n");

    WSADATA wsaData;
    int iResult;

    // Initialize Winsock library (windows sockets)
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    // Prepare temporary data
    SOCKET ConnectSocket  = INVALID_SOCKET;
    struct addrinfo *result = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;
    int iSendResult;
    char recvbuf[512];
    int recvbuflen = sizeof(recvbuf);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the game server address and port
    iResult = getaddrinfo(GAME_SERVER, GAME_SERVER_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    printf("Connecting to game server...\n");
    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
                ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    // Free some used resources
    freeaddrinfo(result);

    // Check if we connected to the game server
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to the game server!\n");
        WSACleanup();
        return 1;
    } else {
        printf("Connected to game server\n");
    }

    AMCOM_Receiver amReceiver;
    AMCOM_InitReceiver(&amReceiver, amPacketHandler, &ConnectSocket);

    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if ( iResult > 0 ) {
            AMCOM_Deserialize(&amReceiver, recvbuf, iResult);
        } else if ( iResult == 0 ) {
            printf("Connection closed\n");
        } else {
            printf("recv failed with error: %d\n", WSAGetLastError());
        }

    } while( iResult > 0 );

    // No longer need the socket
    closesocket(ConnectSocket);
    // Clean up
    WSACleanup();

    return 0;
}
