#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<dirent.h>
#include<fstream>
#include<netdb.h>
#include<unistd.h>
#include<thread>
#include<cstdlib>
#include<time.h>
#include<string.h>
#include<cstring>
#include<iostream>

using namespace std;
#define SERVERPORT 8080 //Listening port no of the server
#define PACKETSIZE 8 // packet size of data being sent/recieved

//declare the server address for the server's socket
struct sockaddr_in ServerAddress;
string ClientUsername;
//client's username

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
// this function is called when server asks for the filename
void searchfile(int sock_fd){
    //recieve filename
    char filename[100];
    int datasize;
    memset(filename,0,sizeof(filename));
    recv(sock_fd,&datasize,sizeof(datasize),0);
    recv(sock_fd,filename,datasize,0);
    //open searching directory and search for file
    struct dirent *d;
    struct stat dst;
    DIR *dir;
    string path = "/shared/public"; 
    dir=opendir(path.c_str());
    bool res =false;
    if(dir!=NULL){
        send(sock_fd,&res,sizeof(res),0);
        
        for(d=readdir(dir);d!=NULL; d=readdir(dir)){
            stat((path+(string)d->d_name).c_str(),&dst);
            if(S_ISDIR(dst.st_mode)){
                continue;
            }
            if(strcmp(d->d_name,filename)){
                res = true;
                break;
            }
        }
    }
    //searching complete send the response to the server
    send(sock_fd,&res, sizeof(res),0);
    closedir(dir);

}

void send_file_list(int Sock_fd){
    int datasize;

    //open searching directory and start searching
    struct dirent *d;
    struct stat dst;
    DIR *dir;

    string path="./shared/public/";
    dir=opendir(path.c_str());

    bool res = false;
    if(dir==NULL){
        send(Sock_fd,&res,sizeof(res),0);
        return;
    }
    for(d=readdir(dir); d!=NULL; d=readdir(dir)){
        stat((path+(string)d->d_name).c_str(), &dst);
        if(S_ISDIR(dst.st_mode)){
            continue;
        }
        res=true;
        send(Sock_fd,&res,sizeof(res),0);
        datasize=strlen(d->d_name);
        send(Sock_fd, &datasize, sizeof(datasize),0);
        send(Sock_fd,d->d_name,datasize,0);
    }
    res=false; // to terminate the loop on server side
    send(Sock_fd, &res, sizeof(res),0);

    //close the directory
    closedir(dir);

}

void sharepublic(int sock_fd){
    //recieve filename
    int datasize;
    char filename[100];
    memset(filename,0,sizeof(filename));
    recv(sock_fd,&datasize,sizeof(datasize),0);
    recv(sock_fd,filename,datasize,0);

    //cout<<filename<<endl;

    //open and send to the file
    fstream fin;
    string pathoffile="./shared/public"+(string)filename; //./shared/public/test
    fin.open(pathoffile,ios::in|ios::out);

    //calculate the size of file
    fin.seekg(0,ios::beg);
    unsigned long long int begin=fin.tellg();
    fin.seekg(0,ios::end);
    unsigned long long int end=fin.tellg();
    unsigned long long int filesize=end-begin;
    send(sock_fd,&filesize,sizeof(filesize),0);

    //start sending the file
    unsigned long long cnt=filesize/PACKETSIZE,i; //80/8=10packets to send the file
    char tmp[10]={0};
    for(i=0; i<cnt; i++){
        fin.seekg(i*(PACKETSIZE), ios::beg);
        fin.read(tmp,PACKETSIZE);

        send(sock_fd,tmp,PACKETSIZE,0);
        memset(tmp,'\0',sizeof(tmp));

    }
    cnt=filesize%PACKETSIZE;
    fin.seekg((i*PACKETSIZE),ios::beg);
    fin.read(tmp,cnt);
    send(sock_fd,tmp,cnt,0);

    //close the file
    fin.close();


}

void download(int sock_fd, string filename){
    cout<<"Downloading....\n";

    //recieve filesize
    unsigned long long int filesize;
    recv(sock_fd,&filesize,sizeof(filesize),0);

    //prepare the output file
    fstream fout;
    fout.open(filename.c_str(),ios::out|ios::in|ios::trunc);

    //download file
    unsigned long long int cnt=filesize/PACKETSIZE,i;
    char tmp[10]={0};
    for(i=0; i<cnt; i++){
        recv(sock_fd,tmp, PACKETSIZE,0);

        fout.write(tmp,PACKETSIZE);
        memset(tmp,'\0',sizeof(tmp));
    }
    cnt=filesize%PACKETSIZE;
    recv(sock_fd,tmp,cnt,0);
    fout.write(tmp,cnt);

    //close the file 
    fout.close();
    cout<<"Download completed....\n";
    cout<<"filename: "<<filename<<" :size: "<<filesize<<"bytes"<<endl;
}

/*
    function: if a succesful match for the file is found,we shall 
    procee with downloading the file
*/

void try_download(string filename){
    // create socket
    int Sock_fd;
    if(Sock_fd=socket(AF_INET,SOCK_STREAM,0)<0){
        cout<<"Socket Creation Error!!\n";
        exit(1);
    }
    if(connect(Sock_fd,(struct sockaddr*)&ServerAddress,sizeof(ServerAddress))<0){
        cout<<"Cannot connect to the server!!\n";
        return;
    }
    //send the requestid

    int req=2;
    send(Sock_fd,&req, sizeof(req),0);

    //send username
    int datasize=ClientUsername.length();
    send(Sock_fd,&datasize,sizeof(datasize),0);
    send(Sock_fd,ClientUsername.c_str(),datasize,0);

    //send filename
    datasize=filename.length();
    send(Sock_fd,&datasize,sizeof(datasize),0);
    send(Sock_fd,ClientUsername.c_str(),datasize,0);

    cout<<"Searching for file...\n";

    //recieve response
    bool res;
    recv(Sock_fd,&res, sizeof(res),0);
    if(res==false){
        cout<<"File no found\n";
        close(Sock_fd);
        return;
    }
    else{
        cout<<"File Found\n";
    }

    //recieve sender's ip and port no
    char senderip[100],senderport;
    //datasize=sizeof(senderip);
    memset(senderip,0,sizeof(senderip));
    recv(Sock_fd,&datasize,sizeof(datasize),0);
    recv(Sock_fd,&senderip,datasize,0);
    recv(Sock_fd,&senderport,sizeof(senderport),0);
    close(Sock_fd);

    //connect to the sending/peer client
    struct sockaddr_in senderaddr;
    memset(&senderaddr,0,sizeof(senderaddr));
    senderaddr.sin_family=AF_INET;
    senderaddr.sin_addr.s_addr=inet_addr(senderip);
    senderaddr.sin_port=htons(senderport);

    if((Sock_fd=socket(AF_INET,SOCK_STREAM,0))<0){
        cout<<"Client req cannot be created!!\n";
        exit(1);
    }
    if(connect(Sock_fd,(struct sockaddr*)&senderaddr,sizeof(senderaddr))<0){
        cout<<"Cannot Connect!!\n";
        exit(1);
    }
    cout<<"Connected with: "<<senderip<<" : "<<senderport<<endl;

    //download the file
    //send the request to the client
    req=1;
    send(Sock_fd,&req,sizeof(req),0);

    //send the filename to the sending client
    datasize=filename.length();
    send(Sock_fd,&datasize,sizeof(datasize),0);
    send(Sock_fd,filename.c_str(),datasize,0);

    download(Sock_fd,filename);

    close(Sock_fd);
}


void getfilelist(){
    //connect to the server
    int sock_fd;
    if(sock_fd=socket(AF_INET,SOCK_STREAM,0)<0){
        cout<<"Error in creation of socket!!\n";
        return;
    }
    if(connect(sock_fd, (struct sockaddr*)&ServerAddress, sizeof(ServerAddress))<0){
        cout<<"Cannot connect to server!!\n";
        close(sock_fd);
        return;
    }
    //send the requestid
    cout<<"Sending request....\n";
    int req=3;
    send(sock_fd,&req, sizeof(req),0);

    //send the username
    int datasize=ClientUsername.length();
    send(sock_fd, ClientUsername.c_str(),datasize,0);

    cout<<"Fetching File List...\n";
    bool next_user=false, next_file=false;
    recv(sock_fd,&next_user,sizeof(next_user),0);
    
    if(next_user==false){
        cout<<"No File found\n";
        close(sock_fd);
        return;
    }
    cout<<"Filess available in the nextwork: "<<endl;
    char filename[100],clientname[100];
    while(next_user==true){
        //recieve the username of client
        memset(clientname,0,sizeof(clientname));
        recv(sock_fd,&datasize,sizeof(datasize),0);
        recv(sock_fd,clientname,datasize,0);

        cout<<"////ClientName: "<<clientname<<"////"<<endl;
        for(recv(sock_fd,&next_file,sizeof(next_file),0);next_file==true;recv(sock_fd,&next_file,sizeof(next_file),0)){
            recv(sock_fd,&datasize, sizeof(datasize),0);
            recv(sock_fd, filename,sizeof(filename),0);

            filename[datasize]='\0';
            cout<<"-"<<filename<<endl;
        }
        cout<<"\n";
        send(sock_fd,&next_user,sizeof(next_user),0);
    }
    close(sock_fd);

}

void user_interface(){
    while(true){
        cout<<"What do you need to perform? "<<endl;
        cout<<"1. Download file\n";
        cout<<"2. List files\n";
        cout<<"3. Refresh Screen\n";
        
        int op;
        cin>>op;
        if(op==1){
            string filename;
            cout<<"Enter the filename to download: ";
            cin>>filename;
            try_download(filename);
        }
        else if(op==2){
            getfilelist();
        }
        else if(op==3){
            continue;
        }
        else{
            cout<<"\nInvalid Choice!!\n";
        }
    }
}

void handle_request(int tmp_fd, int requestid){
    switch (requestid)
    {
    case 0://it is a request from server (to search for a file)
        searchfile(tmp_fd);
        break;
    case 1: //it is a client request
        sharepublic(tmp_fd);
        break;
    case 2:
        //receivefile(tmp_fd);
        break;
    case 3:
        send_file_list(tmp_fd);
        break;
    default:
        cout<<"Invalid Request!!\n";
    }
    close(tmp_fd);
}

void RunClient(){
    int Client_Listen_Sockfd, Client_Request_Sockfd;
    struct sockaddr_in ClientAddress;

    memset(&ClientAddress,0,sizeof(ClientAddress));
    memset(&ServerAddress,0,sizeof(ServerAddress));
    
    //create listening socket
    if((Client_Listen_Sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
        cout<<"Socket could not be created!\n";
        exit(1);
    }

    //fillup the clientaddress
    string myip;
    int myPort;
    cout<<"Enter your ip address: ";
    cin>>myip;
    cout<<"\nEnter your port number: ";
    cin>>myPort;
    cout<<endl;

    ClientAddress.sin_family=AF_INET;
    ClientAddress.sin_addr.s_addr=inet_addr(myip.c_str());
    ClientAddress.sin_port=htons(myPort);

    //binding with socket
    if(bind(Client_Listen_Sockfd, (struct sockaddr*)&ClientAddress, sizeof(ClientAddress)) < 0){
        cout<<"Bind Error!\n";
        exit(1);
    }
    //cout<<"Listening at ipadrress "<<myip<<"portno : "<<myPort<<endl;

    //create the connection request and register with the server

    if((Client_Request_Sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
        cout<<"Client Request Socket could not be created!!\n";
        exit(1);
    }

    string serverip;
    cout<<"Enter your server ip: \n";
    cin>>serverip;

    ServerAddress.sin_family=AF_INET;
    ServerAddress.sin_addr.s_addr=inet_addr(serverip.c_str());
    ServerAddress.sin_port=htons(SERVERPORT);

    if(connect(Client_Request_Sockfd,(struct sockaddr *)&ServerAddress, sizeof(ServerAddress))<0){
        cout<<"Connection could not be established!!\n";
        exit(1);
    }
    cout<<"Connected Successfully....\n";
    //send register request 
    int requestid=1;
    send(Client_Request_Sockfd,&requestid, sizeof(requestid),0);

    cout<<"Enter your username for registration: ";
    cin>>ClientUsername;

    int datasize = ClientUsername.length();
    send(Client_Request_Sockfd,&datasize,sizeof(datasize),0);
    send(Client_Request_Sockfd, ClientUsername.c_str(),datasize,0);

    datasize=myip.length();
    send(Client_Request_Sockfd, &datasize, sizeof(datasize),0);
    send(Client_Request_Sockfd, myip.c_str(),datasize,0);

    send(Client_Request_Sockfd, &myPort, sizeof(myPort),0);

    bool res;
    recv(Client_Listen_Sockfd,&res,sizeof(res),0);
    close(Client_Request_Sockfd);

    if(res==false){
        cout<<"Registration Unsucessful. Try another username!!\n";
        close(Client_Listen_Sockfd);
        exit(1);
    }
    else{
        cout<<"Registration Successful"<<endl;
    }

    //start listen
    if(listen(Client_Listen_Sockfd,6)<0){
        cout<<"Error in Listening!\n";
        exit(1);
    }

    cout<<"Listening ....."<<endl;

    //start userinterface
    std::thread thr_ui(user_interface);
    thr_ui.detach();

    //start accepting requests
    int tmp_sockfd;
    struct sockaddr_in tmpaddress;
    socklen_t addrsize;

    while(true){
        if((tmp_sockfd=accept(Client_Listen_Sockfd,(struct sockaddr*)&tmpaddress,&addrsize))<0){
            cout<<"Accepting Error!\n";
            exit(1);
        }
        
        //recieve requestid
        recv(tmp_sockfd, &requestid, sizeof(requestid),0);

        //handle request
        std::thread thr_hr(handle_request, tmp_sockfd,requestid);
        thr_hr.detach();
    }
    
}
