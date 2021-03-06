#include<netinet/in.h>
#include<errno.h>
#include<netdb.h>
#include<stdio.h> 
#include<stdlib.h>    
#include<string.h>  
#include<net/if_arp.h>
#include<netinet/ip_icmp.h>  
#include<netinet/udp.h>   
#include<netinet/tcp.h>  
#include<netinet/ip.h>   
#include<netinet/if_ether.h>  
#include<net/ethernet.h>  
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<sys/time.h>
#include<sys/types.h>
#include<unistd.h>

 
void ProcessPacket(unsigned char* , int);
void print_ethernet_header(unsigned char* , int );
void print_ip_header(unsigned char* , int);
void print_tcp_packet(unsigned char * , int );
void print_udp_packet(unsigned char * , int );
void print_icmp_packet(unsigned char* , int );
void PrintData (unsigned char* , int);
void print_others(unsigned char*, int);
void print_arp(unsigned char*, int);
void print_raw(unsigned char*, int);
 
FILE *logfile;
struct sockaddr_in source,dest;
int tcp=0,udp=0,icmp=0,others=0,igmp=0,total=0,i,j,dumped=0; 
char monitor[128] = "\0";
 
int main()
{
	char *pos;
	printf("Monitor IP: ");
	fflush(stdin);
	fgets(monitor,sizeof(monitor),stdin);
	fflush(stdin);
	if((pos=strchr(monitor,'\n')) != NULL){*pos = '\0';}
	strtok(monitor,"\n");

	int saddr_size , data_size;
	struct sockaddr saddr;
         
	unsigned char *buffer = (unsigned char *) malloc(65536); //Its Big!
     
	logfile=fopen("log.txt","w");
	if(logfile==NULL) 
	{
		printf("Unable to create log.txt file.");
	}
	printf("Starting sniffer\nCapturing %s...\n",monitor);
	int sock_raw = socket( AF_PACKET , SOCK_RAW , htons(ETH_P_ALL)) ;
	//setsockopt(sock_raw , SOL_SOCKET , SO_BINDTODEVICE , "eth0" , strlen("eth0")+ 1 );
     
	if(sock_raw < 0)
	{
		perror("Socket Error");
		return 1;
	}
	while(1)
	{      
		saddr_size = sizeof saddr;
		data_size = recvfrom(sock_raw , buffer , 65536 , 0 , &saddr , (socklen_t*)&saddr_size);
		if(data_size <0 )
		{
			printf("Recvfrom error , failed to get packets\n");
			return 1;
		}
		//Now process the packet
		ProcessPacket(buffer , data_size);
	}
	close(sock_raw);
	printf("Finished");
	return 0;
}
 
void ProcessPacket(unsigned char* buffer, int size)
{
	
	struct ethhdr *eth = (struct ethhdr *)buffer;
	struct iphdr *iph = (struct iphdr*)(buffer + sizeof(struct ethhdr));
	unsigned short iphdrlen;
	iphdrlen = iph->ihl*4;
	struct tcphdr *tcph=(struct tcphdr*)(buffer + iphdrlen + sizeof(struct ethhdr));
	
	memset(&source, 0, sizeof(source));
	source.sin_addr.s_addr = iph->saddr;

	memset(&dest, 0, sizeof(dest));
	dest.sin_addr.s_addr = iph->daddr;

	if (ntohs(tcph->source) == 22 || ntohs(tcph->dest) == 22){return;}
	if (monitor[0] != '\0'){
		if (strcmp(inet_ntoa(source.sin_addr),monitor) != 0 && strcmp(inet_ntoa(dest.sin_addr),monitor) != 0 )
		{
			//printf("%s %s %d %d %d\n",inet_ntoa(source.sin_addr),monitor,strcmp(inet_ntoa(source.sin_addr),monitor),strlen(inet_ntoa(source.sin_addr)),strlen(monitor));
			return;
		} 
	}
	
	if (strcmp(inet_ntoa(source.sin_addr),"192.168.0.12") == 0 || strcmp(inet_ntoa(dest.sin_addr),"192.168.0.12") == 0 ){dumped++;return;} 
	switch (eth->h_proto)
	{
		case 0x0608:
		fprintf(logfile , "\n\n***********************ARP*************************\n");
		print_ethernet_header(buffer,size);
		print_arp(buffer,size);
		fprintf(logfile, "\n\n****************************************************\n");
		return;
	}

	switch (iph->protocol) 
	{
	/*    case 1:  //ICMP Protocol
			++icmp;
			print_icmp_packet( buffer , size);
			break;
	*/     
	/*    case 2:  //IGMP Protocol
			++igmp;
			break;
	*/     
	case 6:  //TCP Protocol
		++tcp;
		print_tcp_packet(buffer , size);
		break;
         
	case 17: //UDP Protocol
		++udp;
		print_udp_packet(buffer , size);
		break;
         
	default: //Some Other Protocol like ARP etc.
		++others;
		print_others(buffer,size);
	break;
}
	printf("TCP : %d   UDP : %d   ICMP : %d   IGMP : %d   Others : %d   Total : %d	Dumped : %d \r", tcp , udp , icmp , igmp , others , total,dumped);
}
void print_others(unsigned char* Buffer, int Size)
{
	fprintf(logfile , "\n\n***********************Others*************************\n");
	print_ip_header(Buffer,Size);
	fprintf(logfile , "\n\n******************************************************\n");
} 
void print_ethernet_header(unsigned char* Buffer, int Size)
{
	struct ethhdr *eth = (struct ethhdr *)Buffer;
     
	fprintf(logfile , "\n");
	fprintf(logfile , "Ethernet Header\n");
	fprintf(logfile , "   |-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth->h_dest[0] , eth->h_dest[1] , eth->h_dest[2] , eth->h_dest[3] , eth->h_dest[4] , eth->h_dest[5] );
	fprintf(logfile , "   |-Source Address      : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth->h_source[0] , eth->h_source[1] , eth->h_source[2] , eth->h_source[3] , eth->h_source[4] , eth->h_source[5] );
	fprintf(logfile , "   |-Protocol            : %u \n",(unsigned short)eth->h_proto);
}
void print_arp(unsigned char* Buffer, int Size)
{
	int op_index = -1;
	struct ether_arp *arph = (struct ether_arp*)(Buffer + sizeof(struct ethhdr));
	if (ntohs(arph->ea_hdr.ar_op) == ARPOP_REQUEST){op_index = 1;fprintf(logfile,"ARP Request \n");}
	else if (ntohs(arph->ea_hdr.ar_op) == ARPOP_REPLY){op_index = 2; fprintf(logfile,"ARP Reply \n");}
	
	fprintf(logfile,"Sender MAC: "); 
	for(i=0; i<6;i++)
		fprintf(logfile,"%02X:", arph->arp_sha[i]); 

	fprintf(logfile,"\nSender IP: "); 
	for(i=0; i<4;i++)
		fprintf(logfile,"%d.", arph->arp_spa[i]); 

	fprintf(logfile,"\nTarget MAC: "); 
	for(i=0; i<6;i++)
		fprintf(logfile,"%02X:", arph->arp_tha[i]); 

	fprintf(logfile,"\nTarget IP: "); 
		for(i=0; i<4; i++)
	fprintf(logfile,"%d.", arph->arp_tpa[i]);
	fprintf(logfile,"\n"); 
	
	switch (op_index){
		case 1:
			fprintf(logfile, "Who has %d.%d.%d.%d Please tell %d.%d.%d.%d \n",arph->arp_tpa[0],arph->arp_tpa[1],arph->arp_tpa[2],arph->arp_tpa[3],arph->arp_spa[0],arph->arp_spa[1],arph->arp_spa[2],arph->arp_spa[3]);
			break;
		case 2:
			fprintf(logfile, "%d.%d.%d.%d is at %.2X-%.2X-%.2x-%.2x-%.2x-%.2xi \n",arph->arp_tpa[0],arph->arp_tpa[1],arph->arp_tpa[2],arph->arp_tpa[3],arph->arp_tha[0],arph->arp_tha[1],arph->arp_tha[2],arph->arp_tha[3],arph->arp_tha[4],arph->arp_tha[5]);
			break;
		default:
			fprintf(logfile, "unknown operation code");
			break;
	}
	fprintf(logfile,"\n"); 

	
}
void print_ip_header(unsigned char* Buffer, int Size)
{
    print_ethernet_header(Buffer , Size);
         
    struct iphdr *iph = (struct iphdr *)(Buffer  + sizeof(struct ethhdr) );
     
    memset(&source, 0, sizeof(source));
    source.sin_addr.s_addr = iph->saddr;
     
    memset(&dest, 0, sizeof(dest));
    dest.sin_addr.s_addr = iph->daddr;
     
    fprintf(logfile , "\n");
    fprintf(logfile , "IP Header\n");
    fprintf(logfile , "   |-IP Version        : %d\n",(unsigned int)iph->version);
    fprintf(logfile , "   |-IP Header Length  : %d DWORDS or %d Bytes\n",(unsigned int)iph->ihl,((unsigned int)(iph->ihl))*4);
    fprintf(logfile , "   |-Type Of Service   : %d\n",(unsigned int)iph->tos);
    fprintf(logfile , "   |-IP Total QLength   : %d  Bytes(Size of Packet)\n",ntohs(iph->tot_len));
    fprintf(logfile , "   |-Identification    : %d\n",ntohs(iph->id));
    fprintf(logfile , "   |-TTL      : %d\n",(unsigned int)iph->ttl);
    fprintf(logfile , "   |-Protocol : %d\n",(unsigned int)iph->protocol);
    fprintf(logfile , "   |-Checksum : %d\n",ntohs(iph->check));
    fprintf(logfile , "   |-Source IP        : %s\n",inet_ntoa(source.sin_addr));
    fprintf(logfile , "   |-Destination IP   : %s\n",inet_ntoa(dest.sin_addr));
}
 
void print_tcp_packet(unsigned char* Buffer, int Size)
{
    unsigned short iphdrlen;
     
    struct iphdr *iph = (struct iphdr *)( Buffer  + sizeof(struct ethhdr) );
    iphdrlen = iph->ihl*4;
     
    struct tcphdr *tcph=(struct tcphdr*)(Buffer + iphdrlen + sizeof(struct ethhdr));
             
    int header_size =  sizeof(struct ethhdr) + iphdrlen + tcph->doff*4;
     
    fprintf(logfile , "\n\n***********************TCP Packet*************************\n");  
         
    print_ip_header(Buffer,Size);
         
    fprintf(logfile , "\n");
    fprintf(logfile , "TCP Header\n");
    fprintf(logfile , "   |-Source Port      : %u\n",ntohs(tcph->source));
    fprintf(logfile , "   |-Destination Port : %u\n",ntohs(tcph->dest));
    fprintf(logfile , "   |-Sequence Number    : %u\n",ntohl(tcph->seq));
    fprintf(logfile , "   |-Acknowledge Number : %u\n",ntohl(tcph->ack_seq));
    fprintf(logfile , "   |-Header Length      : %d DWORDS or %d BYTES\n" ,(unsigned int)tcph->doff,(unsigned int)tcph->doff*4);
    fprintf(logfile , "   |-Urgent Flag          : %d\n",(unsigned int)tcph->urg);
    fprintf(logfile , "   |-Acknowledgement Flag : %d\n",(unsigned int)tcph->ack);
    fprintf(logfile , "   |-Push Flag            : %d\n",(unsigned int)tcph->psh);
    fprintf(logfile , "   |-Reset Flag           : %d\n",(unsigned int)tcph->rst);
    fprintf(logfile , "   |-Synchronise Flag     : %d\n",(unsigned int)tcph->syn);
    fprintf(logfile , "   |-Finish Flag          : %d\n",(unsigned int)tcph->fin);
    fprintf(logfile , "   |-Window         : %d\n",ntohs(tcph->window));
    fprintf(logfile , "   |-Checksum       : %d\n",ntohs(tcph->check));
    fprintf(logfile , "   |-Urgent Pointer : %d\n",tcph->urg_ptr);
    fprintf(logfile , "\n");
    fprintf(logfile , "                        DATA Dump                         ");
    fprintf(logfile , "\n");
         
    fprintf(logfile , "IP Header\n");
    PrintData(Buffer,iphdrlen);
         
    fprintf(logfile , "TCP Header\n");
    PrintData(Buffer+iphdrlen,tcph->doff*4);
         
    fprintf(logfile , "Data Payload\n");
    PrintData(Buffer + header_size , Size - header_size );
    
    fprintf(logfile , "Data RAW\n");
    print_raw(Buffer + header_size , Size - header_size );                         
    fprintf(logfile , "\n###########################################################");
}
 
void print_raw(unsigned char *data,int size) {
	for (i = 0 ; i <size; i++ ){
		fprintf(logfile , "%c",(unsigned char)data[i]);
    	}
}
void print_udp_packet(unsigned char *Buffer , int Size)
{
     
    unsigned short iphdrlen;
     
    struct iphdr *iph = (struct iphdr *)(Buffer +  sizeof(struct ethhdr));
    iphdrlen = iph->ihl*4;
     
    struct udphdr *udph = (struct udphdr*)(Buffer + iphdrlen  + sizeof(struct ethhdr));
     
    int header_size =  sizeof(struct ethhdr) + iphdrlen + sizeof udph;
     
    fprintf(logfile , "\n\n***********************UDP Packet*************************\n");
     
    print_ip_header(Buffer,Size);           
     
    fprintf(logfile , "\nUDP Header\n");
    fprintf(logfile , "   |-Source Port      : %d\n" , ntohs(udph->source));
    fprintf(logfile , "   |-Destination Port : %d\n" , ntohs(udph->dest));
    fprintf(logfile , "   |-UDP Length       : %d\n" , ntohs(udph->len));
    fprintf(logfile , "   |-UDP Checksum     : %d\n" , ntohs(udph->check));
     
    fprintf(logfile , "\n");
    fprintf(logfile , "IP Header\n");
    PrintData(Buffer , iphdrlen);
         
    fprintf(logfile , "UDP Header\n");
    PrintData(Buffer+iphdrlen , sizeof udph);
         
    fprintf(logfile , "Data Payload\n");    
     
    //Move the pointer ahead and reduce the size of string
    PrintData(Buffer + header_size , Size - header_size);
    fprintf(logfile , "Data RAW\n");
    print_raw(Buffer + header_size , Size - header_size );
     
    fprintf(logfile , "\n###########################################################");
}
 
void print_icmp_packet(unsigned char* Buffer , int Size)
{
    unsigned short iphdrlen;
     
    struct iphdr *iph = (struct iphdr *)(Buffer  + sizeof(struct ethhdr));
    iphdrlen = iph->ihl * 4;
     
    struct icmphdr *icmph = (struct icmphdr *)(Buffer + iphdrlen  + sizeof(struct ethhdr));
     
    int header_size =  sizeof(struct ethhdr) + iphdrlen + sizeof icmph;
     
    fprintf(logfile , "\n\n***********************ICMP Packet*************************\n"); 
     
    print_ip_header(Buffer , Size);
             
    fprintf(logfile , "\n");
         
    fprintf(logfile , "ICMP Header\n");
    fprintf(logfile , "   |-Type : %d",(unsigned int)(icmph->type));
             
    if((unsigned int)(icmph->type) == 11)
    {
        fprintf(logfile , "  (TTL Expired)\n");
    }
    else if((unsigned int)(icmph->type) == ICMP_ECHOREPLY)
    {
        fprintf(logfile , "  (ICMP Echo Reply)\n");
    }
     
    fprintf(logfile , "   |-Code : %d\n",(unsigned int)(icmph->code));
    fprintf(logfile , "   |-Checksum : %d\n",ntohs(icmph->checksum));
    fprintf(logfile , "\n");
 
    fprintf(logfile , "IP Header\n");
    PrintData(Buffer,iphdrlen);
         
    fprintf(logfile , "UDP Header\n");
    PrintData(Buffer + iphdrlen , sizeof icmph);
         
    fprintf(logfile , "Data Payload\n");    
     
    //Move the pointer ahead and reduce the size of string
    PrintData(Buffer + header_size , (Size - header_size) );
     
    fprintf(logfile , "\n###########################################################");
}
 
void PrintData (unsigned char* data , int Size)
{
    int i , j;
    for(i=0 ; i < Size ; i++)
    {
        if( i!=0 && i%16==0)   //if one line of hex printing is complete...
        {
            fprintf(logfile , "         ");
            for(j=i-16 ; j<i ; j++)
            {
                if(data[j]>=32 && data[j]<=128)
                    fprintf(logfile , "%c",(unsigned char)data[j]); //if its a number or alphabet
                 
                else fprintf(logfile , "."); //otherwise print a dot
            }
            fprintf(logfile , "\n");
        } 
         
        if(i%16==0) fprintf(logfile , "   ");
            fprintf(logfile , " %02X",(unsigned int)data[i]);
                 
        if( i==Size-1)  //print the last spaces
        {
            for(j=0;j<15-i%16;j++) 
            {
              fprintf(logfile , "   "); //extra spaces
            }
             
            fprintf(logfile , "         ");
             
            for(j=i-i%16 ; j<=i ; j++)
            {
                if(data[j]>=32 && data[j]<=128) 
                {
                  fprintf(logfile , "%c",(unsigned char)data[j]);
                }
                else
                {
                  fprintf(logfile , ".");
                }
            }
             
            fprintf(logfile ,  "\n" );
        }
    }
}
