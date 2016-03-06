#include "socket.h"
#include <thread>
#include <pthread.h>

#define LOCALPORT 5500
#define REMOTEPORT 6000

#define SUCCESS 0
#define ERR_SOCKET 3


void setupSocket(Socket *local, sockaddr_in sin_remote,
                 sockaddr_in sin_local, int *aux)
{
    try {
         //*local = Socket(sin_local);
         *local = Socket(sin_local,sin_remote);

    }catch (std::system_error& e) {

        if (errno == ENOBUFS || errno == ENOMEM) {
            std::cerr << program_invocation_name << ": Not enough memory for creating Socket "
            << std::endl;
        } else {
            std::cerr << program_invocation_name << ": " << e.what()
            << std::endl;
        }

        *aux = ERR_SOCKET;    // Error. Termina el programa siempre con un valor > 0
    }
}



void request_cancellation(std::thread& onethread)
{
    pthread_cancel(onethread.native_handle());
}

void getandSendMessage(Socket *local, Message message, sockaddr_in remote,
                       std::string *message_text, bool *endOfLoop)
{
    //We should ead until read quit or eof
    while (!*endOfLoop) {
        std::getline(std::cin,*message_text);

        if(*message_text == "/quit" || std::cin.eof())
            *endOfLoop = true;

        if (!*endOfLoop) {
            message_text->copy(message.text, sizeof(message.text) - 1, 0);
            message.text[message_text->length()] = '\0';
            local->sendTo(reinterpret_cast<const Message&>(message),
                     reinterpret_cast<const sockaddr_in&>(remote));
        }
    }

}

void receiveAndShowMessage(Socket *socket, Message *message, sockaddr_in sin_remote)
{
    while(1) {
        socket->receiveFrom(*message, sin_remote);

        // Mostrar el mensaje recibido en la terminal
        char* remote_ip = inet_ntoa(sin_remote.sin_addr);
        int remote_port = ntohs(sin_remote.sin_port);

        /* Para aumentar la seguridad de la aplicacion,
         * añadimos un fin de cadena que limite la lectura
         * al tamaño del buffer, por si el mensaje no está
         * delimitado de por sí
         */
        message->text[1023] = '\0';

        std::cout << "El sistema " << remote_ip << ":" << remote_port <<
        " envió el mensaje '" << message->text << "'" << std::endl;
    }

}

void firsThread(Socket *local,Message message, sockaddr_in *sin_remote,
                std::string *message_text,bool *endOfLoop)
{
    try {
        getandSendMessage(&*local,message,*sin_remote,&*message_text,
                          &*endOfLoop);
    } catch (std::system_error& e) {
        std::cerr << program_invocation_name << ": " << e.what()
        << std::endl;
    }

}

void secondThread(Socket *local,Message *message, sockaddr_in *sin_remote)
{
    try {
        receiveAndShowMessage(&*local,&*message,*sin_remote);
    } catch (std::system_error& e) {
        std::cerr << program_invocation_name << ": " << e.what()
        << std::endl;
    }

}

void startCommunication(Socket *local,sockaddr_in *sin_remote){
    Message message; //Estructura de mensaje
    std::string message_text(""); //String para input

    bool endOfLoop = false;

    std::thread hilo1;
    std::thread hilo2;

    if(local->actingLikeServer())
        local->handleConnections();

    hilo1=std::thread (&firsThread,&*local,
                      message,&*sin_remote,&message_text,
                      &endOfLoop);
    hilo2=std::thread (&secondThread,&*local,&message,&*sin_remote);

    //They shall no block each other
    hilo1.detach();
    hilo2.detach();

    while(!endOfLoop);
    //We must finish both threads gracefully!
    request_cancellation(hilo1);
    request_cancellation(hilo2);
}

int main(void){

    int aux = SUCCESS;
    //Preparar estructura local y remoto
    sockaddr_in sin_local = makeIpAddress("0.0.0.0",LOCALPORT);
    sockaddr_in sin_remote = makeIpAddress("0.0.0.0",REMOTEPORT);

    Socket local;

    setupSocket(&local,sin_local,sin_remote,&aux);

    if(aux == SUCCESS) {

        startCommunication(&local,&sin_remote);
    }

    return aux;
}

