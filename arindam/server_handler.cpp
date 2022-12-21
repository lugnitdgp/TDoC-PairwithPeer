#include<iostream>
#include<map>
#include<vector>
#include<sys/socket.h>
#include<string>
#include<cstring>
#include<mutex>
#include<thread>
#include<string.h>
#include<cstdlib>
#include<fstream>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fstream>
#include<sys/file.h>
#include<sys/types.h>
#include<unistd.h>
using namespace std;

//globally declare port no
#define PORT 8080 //the listening port of the server
#define PACKET_SIZE 8 //size of the data being transferred

//declare the client list 
map<string, pair<string,int> >Client_List; //store the client name, (the ip address and port no) in a map

// define mutex

mutex mtx;

//************ Functions **********//  
string getip(){
    FILE *f=popen("ip a | grep 'scope global' | grep -v ':' | awk '{print$2}' | cut -d '/' -fl", "r");
    char c;string s="";
    while((c=getc(f))!=EOF){
        s+=c;
    }
    pclose(f);
    //cout<<s<<endl;
    return s;
}

void Register_Client(int Sock_fd){
    char Client_Name[100], Client_ipaddress[100];
    int Client_port;
    memset(Client_Name,0,100);
    memset(Client_ipaddress,0,100);
    int datasize=0;

    //client's username
    recv(Sock_fd, &datasize, sizeof(datasize),0);
    recv(Sock_fd, &Client_Name, sizeof(Client_Name),0);


    //client's ipadress
    recv(Sock_fd, &datasize, sizeof(datasize),0);
    recv(Sock_fd, &Client_ipaddress, sizeof(Client_ipaddress),0);

    //client's port number
    recv(Sock_fd, &Client_port, sizeof(Client_port),0);

    //register client
    //critical section we need to use mutex
    mtx.lock();
    string username = Client_Name;
    pair<string, int>clientinfo;
    clientinfo = {(string)Client_ipaddress,Client_port};
    std::map<string, pair<string, int> >::iterator it;
    it=Client_List.find(username);
    if(it!=Client_List.end()){
        bool res = false;
        send(Sock_fd, &res, sizeof(res),0);
        mtx.unlock();
    }
    Client_List[username]=clientinfo;
    bool res = true;
    send(Sock_fd,&res, sizeof(res),0);
    //display client's list
    for(auto it:Client_List){
        cout<<it.first<<": "<<it.second.first<<": "<<it.second.second<<endl;
    }
    mtx.unlock();
}

/*
    return: true-> file is found
            false->file is not found
    fetches the filename which the client has requested, this filename will be sent to all
    other users check if file is found on other clients. if match is found we will 
    ask for the port no and ipaddress of the client and sent it to the requesting client

*/

bool Handle_Download_Request(int Sock_fd){
    int temp_fd,datasize=0;
    struct sockaddr_in Client_Address;
    bool response;

    memset(&Client_Address,0, sizeof(Client_Address)); //for clear

    char filename[100],username[100];
    memset(&filename,0,sizeof(filename));
    memset(&username,0,sizeof(username));

    // receive file and username of client
    recv(Sock_fd,&datasize, sizeof(datasize),0);
    recv(Sock_fd,&username,sizeof(username),0);

    std::map<string,pair<string, int> >::iterator it;
    it=Client_List.find(username);
    while(it->first!=username){
        Client_Address.sin_family=AF_INET;
        Client_Address.sin_addr.s_addr=inet_addr(it->second.first.c_str());
        Client_Address.sin_port=htons(it->second.second);

        //create temporary socket
        if((temp_fd = socket(AF_INET,SOCK_STREAM,0))<0){
            continue;
        }
        //connecting the temporary socket
        if(connect(temp_fd,(struct sockaddr*)&Client_Address,sizeof(Client_Address))<0){
            continue;
        }

        // send request
        int req=0;
        send(temp_fd, &req, sizeof(req),0);

        //send filename
        datasize=strlen(filename);
        send(temp_fd,&datasize,sizeof(datasize),0);
        send(temp_fd,&filename,datasize,0);

        //recieve response
        recv(temp_fd, &response, sizeof(response),0);

        if(response==true) //file is found
        {
            //forward the response to requesting client
            send(Sock_fd,&response,sizeof(response),0);

            //send the ipaddress and portno of the client where file is present
            datasize=it->second.first.length();
            send(Sock_fd,&datasize, sizeof(datasize),0);
            send(Sock_fd,it->second.first.c_str(),datasize,0);
            send(Sock_fd,&(it->second.second), sizeof(it->second.second),0);

            // the work is done
            close(temp_fd); // close the socket
            return true;
        }
        close(temp_fd);
        if(it==Client_List.end()){
            it=Client_List.begin();
        }
        it++;
    }
    return false;
}

void Handle_List_Request(int Sock_fd){
    int temp_fd, datasize=0;
    char username[100],filename[100];
    bool next_user,next_file;
    struct sockaddr_in Client_addr;
    memset(&username,0,sizeof(username));
    
    //get the username of the requesting client
    recv(Sock_fd,&datasize,sizeof(datasize),0);
    recv(Sock_fd,&username,sizeof(username),0);

    std::map<string,pair<string, int> >::iterator it;
    
    for(it=Client_List.begin(); it!=Client_List.end(); it++,next_user=false){
        if(it->first==(string)username){
            continue;
        }

        if((temp_fd=socket(AF_INET,SOCK_STREAM,0))<0){
            next_user=false;
            send(Sock_fd,&next_user,sizeof(next_user),0);
            cout<<"unexpected error occurred!"<<endl;
            return;
        }

        memset(&Client_addr,0,sizeof(Client_addr));
        Client_addr.sin_family=AF_INET;
        Client_addr.sin_addr.s_addr=inet_addr(it->second.first.c_str());
        Client_addr.sin_port=htons(it->second.second);

        if(connect(temp_fd, (struct sockaddr*)&Client_addr,sizeof(Client_addr))<0){
            next_user=false;
            send(Sock_fd,&next_user,sizeof(next_user),0);
            cout<<"unexpected error occurred!"<<endl;
            return;
        }
        next_user=true;
        send(Sock_fd,&next_user,sizeof(next_user),0);

        datasize=it->first.size();
        send(Sock_fd,&datasize,sizeof(datasize),0);
        send(Sock_fd,it->first.c_str(),datasize,0);

        int req=3;
        send(temp_fd,&req,sizeof(req),0);
        datasize=100;
        for(recv(temp_fd,&next_file,sizeof(next_file),0); next_file==true;recv(temp_fd, &next_file,sizeof(next_file),0)){
            memset(filename,0,datasize);

            //recieve the filename
            recv(temp_fd, &datasize,sizeof(datasize),0);
            recv(temp_fd,&filename,sizeof(filename),0);

            send(Sock_fd,&next_file,sizeof(next_file),0);
            send(Sock_fd,&datasize, sizeof(datasize),0);
            send(Sock_fd,&filename,datasize,0);
        }
        close(temp_fd);
    }
    
}

void Handle_Client_Request(int Sockfd, int RequestID){
    switch (RequestID)
    {
    case 1: //new connection request 
        Register_Client(Sockfd);
        break;
    case 2: //request to download a file
        if(Handle_Download_Request(Sockfd)==false){
            bool check=false;
            send(Sockfd,&check, sizeof(check),0);
        }
        break;
    case 3: //Request List of Files
        Handle_List_Request(Sockfd);
        break;

    default:
        cout<<"Invalid Request!\n";
    }
    //close the socket
    close(Sockfd);
    return;
}

void RunServer(){
    int Server_Sockfd, New_Client_Sockfd;
    struct sockaddr_in Server_Address;
    socklen_t addr_size;
    addr_size = sizeof(Server_Address);

    memset(&Server_Address,0,addr_size); //clear the Server_address

    //create socket
    if((Server_Sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
        cout<<"Socket Creation Error!\n";
        exit(1);
    }

    //fill up the server_address
    string ipaddr=getip();
    Server_Address.sin_family=AF_INET;
    Server_Address.sin_port=htons(PORT);

    //bind
    if((bind(Server_Sockfd,(struct sockaddr*)&Server_Address, addr_size))<0){
        cout<<"Binding Error!\n";
        exit(1);
    }

    //listen
    if(listen(Server_Sockfd,10)<0){
        cout<<"Cannot Listen!\n";
        exit(1);
    }

    cout<<"Listening at: "<<ipaddr<<" and portno: "<<PORT;
    //accept the requests from the client

    while(true){
        if((New_Client_Sockfd=accept(Server_Sockfd, (struct sockaddr*)&Server_Address, &addr_size))<0){
            cout<<"Connection Could not be established!!\n";
            exit(1);
        }
        int RequestID;
        recv(New_Client_Sockfd, &RequestID, sizeof(RequestID),0);

        //cout<<RequestID<<endl;

        std::thread thr(Handle_Client_Request,New_Client_Sockfd,RequestID);

    }

}

