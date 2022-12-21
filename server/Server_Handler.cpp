#include<iostream>
#include<map>
#include<vector>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<cstring>
#include<mutex>
#include <thread>
#include<string>
#include<cstdlib>
#include<fstream>
#include<unistd.h>

using namespace std;

#define PORT 8080
#define PACKETSIZE 8 //SIZE OF THE DATA BEING TRANSFERRED

//declare the client list 
map<string,pair<string,int>> Client_List; //ip address and port number mapped to the username
//define Mutex
mutex mtx;


//FUNCTIONS

string getip()
{
    FILE *f = popen("ip a | grep 'scope global' | grep -v ':' | awk '{print $2}' | cut -d '/' -f1", "r");
    char c;
    string s = "";
    while((c=getc(f))!=EOF)
    {
        s+=c;
    }
    // cout<<s<<endl;
    pclose(f);
    return s;
}
void Register_Client(int Sockfd)
{
    char Client_Name[100] , Client_ipaddress[100],Client_port;
    memset(Client_Name,0,100);
    memset(Client_ipaddress,0,100);
    int datasize = 0;

    //Client's username
    recv(Sockfd,&datasize,sizeof(datasize),0);
    recv(Sockfd,&Client_Name,sizeof(Client_Name),0);

    //Client's IPaddress
    recv(Sockfd,&datasize,sizeof(datasize),0);
    recv(Sockfd,&Client_ipaddress,sizeof(datasize),0);

    //Client's Port Number
    recv(Sockfd,&Client_port,sizeof(datasize),0);

    //Register Client
    //Critical Section we need to use Mutex
    mtx.lock();
    string username = Client_Name;
    pair<string,int>clientinfo = {Client_ipaddress,Client_port};
    auto it = Client_List.find(username);

    if(it!=Client_List.end())
    {
        bool res = 0;
        send(Sockfd,&res,sizeof(res),0);
        mtx.unlock();
        return;
    }

    Client_List[username] = clientinfo;
    cout<<"HOla"<<endl;
    bool res = true;
    send(Sockfd,&res,sizeof(res),0);
    //Display the number of clients currently connected to the server
    for(auto it:Client_List)
    {
        cout<<it.first<<" "<<it.second.first<<" "<<it.second.second<<endl;
    }
    mtx.unlock();

}
/* returns true if the file is found else returns false
-fetches the filename which the client has requested and will send this
to other users and check if it is present or not on other clients.If the match
is found we will ask for  port number  and ipaddress of the client and send it to 
the requesting client.
*/
bool Handle_Download_Request(int Sockfd)
{
    int tempfd,datasize=0;
    struct sockaddr_in Client_Address;
    bool response;

    memset(&Client_Address,0,sizeof(Client_Address));//to clear

    
    char filename[100],username[100];
    memset(&filename,0,sizeof(filename));
    memset(&username,0,sizeof(username));

    //receive filename, and username of client 
    recv(Sockfd,&datasize,sizeof(datasize),0);
    recv(Sockfd,&filename,datasize,0);

    recv(Sockfd,&datasize,sizeof(datasize),0);
    recv(Sockfd,&username,datasize,0);

    auto it = Client_List.find(username);
    it++;

    while(it->first!=username)
    {
        Client_Address.sin_family = AF_INET;
        Client_Address.sin_addr.s_addr = inet_addr(it->second.first.c_str());
        Client_Address.sin_port = htons(it->second.second);

        //creating temporary socket
        if(tempfd=socket(AF_INET,SOCK_STREAM,0)<0)
        {
            cout<<"Wasn't able to create the temporary socket"<<endl;
            continue;
        }

        //connect
        if(connect(tempfd,(struct sockaddr*)&Client_Address,sizeof(Client_Address))<0)
        {
            it++;
            continue;
        }

        //Send request
        int req = 0;
        send(tempfd,&req,sizeof(req),0);

        //Send filename
        datasize = strlen(filename);
        send(tempfd,&datasize,sizeof(datasize),0);
        send(tempfd,&filename,datasize,0);

        //receiving response
        recv(tempfd,&response,sizeof(response),0);

        if(response)//if the file is found
        {
            //forward the response to the requesting client
            send(Sockfd,&response,sizeof(response),0);

            //send the ipaddress and portno of the client where the file is present
            datasize = it->second.first.length();
            send(Sockfd,&datasize,sizeof(datasize),0);
            send(Sockfd,it->second.first.c_str(),datasize,0);
            send(Sockfd,&(it->second.second),sizeof(it->second.second),0);

            //the work is done
            close(tempfd);//closing the socket
            return true;
        }
        //if the response is false
        close(tempfd);
        if(it==Client_List.end())
        {
            it = Client_List.begin();
        }
        it++;

    }
    return false;
}
void Handle_List_Request(int Sockfd)
{
    int tempfd,datasize=0;
    bool next_user = false,next_file;
    char username[100],filename[100];
    struct sockaddr_in Client_Address;

    memset(username,0,sizeof(username));

    //get username
    recv(Sockfd,&datasize,sizeof(datasize),0);
    recv(Sockfd,&username,sizeof(username),0);

    for(auto it = Client_List.begin();it!=Client_List.end();it++,next_user=false)
    {
        if(it->first==username)//omit the filenames for the asking user
        {
            continue;
        }
        

        //creating a temporary socket
        if(tempfd=socket(AF_INET,SOCK_STREAM,0)<0)
        {
            next_user = false;
            send(Sockfd,&next_user,sizeof(next_user),0);
            cout<<"Paradise Lost"<<endl;
            return;
        }  
        memset(&Client_Address,0,sizeof(Client_Address));

        Client_Address.sin_family = AF_INET;
        Client_Address.sin_addr.s_addr=inet_addr(it->second.first.c_str());
        Client_Address.sin_port= htons(it->second.second);

        if(connect(tempfd,(struct sockaddr*)&Client_Address,sizeof(Client_Address))<0)
        {
            next_user =false;
            send(Sockfd,&next_user,sizeof(next_user),0);
            cout<<"No connection"<<endl;
            return;
        }
        next_user = true;
        send(Sockfd,&next_user,sizeof(next_user),0);


        datasize = it->first.size();
        send(Sockfd,&datasize,sizeof(datasize),0);
        send(Sockfd,(it->first.c_str()),datasize,0);

        int req = 3;
        send(tempfd,&req,sizeof(req),0);
        datasize = 100;
        
        for(recv(tempfd,&next_file,sizeof(next_file),0);next_file==true;recv(tempfd,&next_file,sizeof(next_file),0))
        {
            memset(filename,0,datasize);
            recv(tempfd,&datasize,sizeof(datasize),0);
            recv(tempfd,&filename,sizeof(filename),0);

            send(Sockfd,&next_file,sizeof(next_file),0);
            send(Sockfd,&next_file,sizeof(datasize),0);
            send(Sockfd,&filename,datasize,0);
            
        }
        close(tempfd);
    }

}
void Handle_Client_Request(int Sockfd,int RequestID)
{
    switch(RequestID)
    {
        case 1://new connection request(register client operation)
            Register_Client(Sockfd);
            break;
        case 2://requesting to download the file
            
            if(Handle_Download_Request(Sockfd)==false)
            {
                bool check = Handle_Download_Request(Sockfd);
                send(Sockfd,&check,sizeof(check),0);
            }
            break;
        case 3://Request List of Files
            Handle_List_Request(Sockfd);
            break;
        default:
            cout<<"Invalid Request";
            break;
        //close Socket
        close(Sockfd);
        return;
    }
    
}
void RunServer()
{
    int Server_Sockfd , New_Client_Sockfd;
    struct sockaddr_in Server_address;
    socklen_t addr_size;
    addr_size = sizeof(Server_address);

    memset(&Server_address,0,addr_size);

    //socket creation
    Server_Sockfd = socket(AF_INET,SOCK_STREAM,0);

    if(Server_Sockfd<0)
    {
        cout<<"Server socket could not be created";
        exit(1);
    }

    //Fill up the server_address
    string ipaddr = getip();

    Server_address.sin_family = AF_INET;
    Server_address.sin_addr.s_addr= inet_addr(ipaddr.c_str());
    Server_address.sin_port = htons(PORT);

    //bind 
    if(bind(Server_Sockfd,(struct sockaddr*)&Server_address,addr_size)<0)
    {
        cout<<"Binding error";
        exit(1);
    }

    //listen
    if(listen(Server_Sockfd,10)<0)
    {
        cout<<"Cannot Listen";
        exit(1);
    }
    cout<<"Listening  at "<<ipaddr<<"and port number "<<PORT<<endl;
    //accept the requests from the client and separate threads for handling each client

    while(true)
    {
        if(New_Client_Sockfd=accept(Server_Sockfd,(struct sockaddr*)&Server_address,&addr_size)<0)
        {
            cout<<"Connection could not be established"<<endl;
            exit(1);
        }
        int RequestID;
        recv(New_Client_Sockfd,&RequestID,sizeof(RequestID),0);

        cout<<RequestID<<endl;

        std::thread thr(Handle_Client_Request,New_Client_Sockfd,RequestID);
        thr.detach();
    }

    

}
