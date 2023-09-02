#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <math.h>

//params: W, a, lambda, p_data, p_ack
//gcc -o test test_s.c -lm

//Selective repeat
//gcc -o sr sr.c -lm
struct pk_list{
    long sn;//sequence number
    double gentm;//generation time
    double t_out;//timeout
    struct pk_list* link;
    struct pk_list* prev;
};

typedef struct pk_list DataQue;
DataQue *WQ_front, *WQ_rear;//waiting for transmit
DataQue *TransitQ_front, *TransitQ_rear;//transmit, but not ACK yet

struct ack_list{
    long sn;//sequence number
    double ack_rtm;//reception time of an ACK at sender
    struct ack_list* link;
    struct ack_list* prev;
};

typedef struct ack_list AckQue;
AckQue *AQ_front, *AQ_rear;//ACK - receiver sent to sender, but not processed yet by sender


long seq_n=0, transit_pknum = 0;
long next_acksn=0;
double cur_tm=0, next_pk_gentm;
double t_pknum=0, t_delay=0;

//Input params
long N;//simulation time: num of packets to be processed
double timeout_len;
int W;//Sliding window size
float a, t_pk, t_pro;//a is ratio of link propagation time(t_pro) to packet transmission
float lambda;//lambda is packet arrival rate in Poisson dist.
float p_data, p_ack;//p_data and p_ack are packet and ack transmission error probability, respectively.

int SHOW = 0;

float random01(void);
void pk_gen(double);
void suc_transmission(long);
void transmit_pk(void);
void receive_pk(long, double);
void enque_Ack(long);
void cur_tm_update(void);
void cut_data(DataQue*);
void cut_ack(AckQue*);
void print_performance_measure(void);
void show(void);
void memory_dealloacation(void);
void init(void);
void simul(void);

FILE * file;
void W_test(void);
void a_test(void);
void lambda_test(void);
void pdata_test(void);
void pack_test(void);
void high_low_comp(void);

int main(void)
{
    //file = fopen("SR.txt", "w+");
    //gcc -o sr sr.c -lm
    //SHOW=1;
    
    SHOW = 1;
    N = 40;
    timeout_len = 2.0;
    t_pk = 0.1;
    
    p_ack = 0.1;
    p_data = 0.1;
    a=4;
    W=8;
    lambda=10;
    
    t_pro = a*t_pk;
    simul();

    //fclose(file);
    return 0;
}
void high_low_comp(void)
{
    //compare low load vs high load
    N = 10000;//simulation time: num of packets to be processed
    timeout_len = 2.0;
    t_pk = 0.1;
    ///////////////////
    p_ack = 0.01;//high -> high load
    p_data = 0.01;//high -> high load
    a=10;//low -> high load
    W=1;//high -> high load
    lambda=5;//high -> high load
    //////////////////
    t_pro = a*t_pk;
    int n = 100;
    
    while(n--)
    {
        init();
        fprintf(file, "%d	", 100-n);
        simul();
        a -= 0.1;
        W++;
        //////////
        t_pro = a*t_pk;
    }
}

void W_test(void)
{
    N = 10000;//simulation time: num of packets to be processed
    timeout_len = 2.0;//3.0
    t_pk = 0.1;
    ///////////////////
    a=30;
    p_ack = 0.1;
    p_data = 0.1;
    lambda=10.0;
    //////////////////
    t_pro = a*t_pk;
    
    int n = 100;
    W=1;
    while(n--)
    {
        init();
        fprintf(file, "%d	", W);
        simul();
        W++;
    }
}
void a_test(void)
{
    N = 10000;//simulation time: num of packets to be processed
    timeout_len = 2.0;//3.0
    t_pk = 0.1;
    ///////////////////
    p_ack = 0.1;
    p_data = 0.1;
    W=8;
    lambda = 10.0;
    ////////////////////
    
    int n = 100;
    a = 0.1;
    t_pro = a*t_pk;
    while(n--)
    {
        init();
        fprintf(file, "%f	", a);
        simul();

        a+=0.1;
        t_pro = a*t_pk;
    }
}
void lambda_test(void)
{
    N = 10000;//simulation time: num of packets to be processed
    timeout_len = 2.0;//3.0
    t_pk = 0.1;
    ///////////////////
    p_ack = 0.1;
    p_data = 0.1;
    W=8;
    a=4;
    ///////////////////
    t_pro = a*t_pk;
    
    int n = 100;
    lambda = 0.2;
    while(n--)
    {
        init();
        fprintf(file, "%f	", lambda);
        simul();
        lambda+=0.2;
    }
}
void pdata_test(void)
{
    N = 10000;//simulation time: num of packets to be processed
    timeout_len = 2.0;//3.0
    t_pk = 0.1;
    ///////////////////
    p_ack = 0.1;
    W=8;
    a=4;
    lambda=10.0;
    ///////////////////
    t_pro = a*t_pk;
    
    int n = 100;
    p_data=0.0;
    while(n--)
    {
        init();
        fprintf(file, "%f	", p_data);
        simul();
        p_data+=0.005;
    }
}
void pack_test(void)
{
    N = 10000;//simulation time: num of packets to be processed
    timeout_len = 2.0;//3.0
    t_pk = 0.1;
    ///////////////////
    p_data = 0.1;
    W=8;
    a=4;
    lambda=10.0;
    ///////////////////
    t_pro = a*t_pk;
    
    int n = 100;
    p_ack=0.0;
    while(n--)
    {
        init();
        fprintf(file, "%f	", p_ack);
        simul();
        p_ack+=0.005;
    }
}

float random01(void)
{
    return (float)rand() / (float)RAND_MAX;
}

void pk_gen(double tm)
{
    DataQue* ptr;
    ptr = malloc(sizeof(DataQue));
    ptr->sn = seq_n;
    ptr->gentm = tm;
    ptr->t_out = timeout_len;
    ptr->link = NULL;
    ptr->prev = NULL;
    seq_n++;
    if(WQ_front==NULL) WQ_front = ptr;
    else WQ_rear->link = ptr;
    WQ_rear = ptr;
}

void suc_transmission(long sn)
{
    //differ from gbn
    DataQue* ptr;
    AckQue* aptr;
//ack 를 받은 패킷: transQ 에서 제거
//transQ 패킷수 1 감소
//수신한 AQ: AQ 에서 제거.

    ptr = TransitQ_front;
    while(ptr!=NULL)
    {
        if(ptr->sn == sn)
        {
            cut_data(ptr);
            free(ptr);
            transit_pknum--;
            break;
        }
        ptr = ptr->link;
    }
    aptr = AQ_front;
    while(aptr!=NULL)
    {
        if(aptr->sn == sn)
        {
            cut_ack(aptr);
            free(aptr);
            break;
        }
        aptr = aptr->link;
    }
}

void transmit_pk(void)
{
    DataQue* ptr;
    cur_tm += t_pk;

    WQ_front->t_out = cur_tm+timeout_len;
    ptr = WQ_front;
    WQ_front = WQ_front->link;
    if(WQ_front==NULL) WQ_rear = NULL;
    else WQ_front->prev = NULL;//changed

    if(TransitQ_front==NULL) TransitQ_front = ptr;
    else TransitQ_rear->link = ptr;
    ptr->link = NULL;
    ptr->prev = TransitQ_rear;//changed
    TransitQ_rear = ptr;
    //transit_pknum++;//재전송에 의해 같은 걸 한번 더 보낼 때는 transit_pknum++ 하면 안 됨
    
    //W 의 front 를 T 의 rear로.
}

void receive_pk(long seqn, double gtm)
{
    //prob at 1-p - suc
    if(random01() > p_data)
    {
        //data transmit suc
        if(seqn < next_acksn)//받으려고 대기한 seqn 을 받지않음 -> 재전송에 의한것(sn<next) or 데이터전송에러로 중간 건너뜀(next<sn)
        {
            //재전송
            //seqn 만 재전송
            if(random01() > p_ack)
                enque_Ack(seqn);
            //else printf("ack error %ld\n", seqn);
            //t_pknum 변화없음. 똑같은거 재전송이므로.
            return;
        }
        t_delay += cur_tm+t_pro - gtm;
        t_pknum++;//수신측 기준 성공개수. 수신측에서 0번~(N-1)번 패킷을 모두 받으면 시뮬레이션 종료.
        next_acksn++;
        if(random01() > p_ack)//ack transmit suc
            enque_Ack(seqn);
        else if(SHOW==1) printf("ack error %ld\n", seqn);//->next_acksn 포함해서 재설정 go back n
    }
    else if(SHOW==1) printf("data error %ld\n", seqn);
}

void enque_Ack(long seqn)
{
    AckQue *ack_ptr;
    ack_ptr = malloc(sizeof(AckQue));
    ack_ptr->sn = seqn;
    ack_ptr->ack_rtm = cur_tm + 2*t_pro;//->:t_pro, <-:t_pro
    ack_ptr->link = NULL;
    ack_ptr->prev = AQ_rear;//changed
    
    if (AQ_front == NULL)
        AQ_front = ack_ptr;
    else
        AQ_rear->link = ack_ptr;
    AQ_rear = ack_ptr;
}

void cur_tm_update(void)
{
    double tm;
    if((WQ_front!=NULL) && (transit_pknum<W)) return;//보낼 패킷이 존재
    else
    {
        if(AQ_front==NULL) tm = next_pk_gentm;
        else if(AQ_front->ack_rtm < next_pk_gentm) tm = AQ_front->ack_rtm;
        else tm = next_pk_gentm;

        if(TransitQ_front!=NULL)
            if(TransitQ_front->t_out < tm)
                tm = TransitQ_front->t_out;
        
        if(tm > cur_tm) cur_tm = tm;
    }
}
void cut_data(DataQue* data)
{
    if(data==NULL) return;
    else if(data==TransitQ_front)
    {
        TransitQ_front = TransitQ_front->link;
        if(TransitQ_front!=NULL) TransitQ_front->prev = NULL;//2개 이상
        else TransitQ_rear = NULL;//1개
    }
    else if(data==TransitQ_rear)
    {
        //2개 이상
        TransitQ_rear = TransitQ_rear->prev;
        TransitQ_rear->link = NULL;
    }
    else
    {
        //3개 이상
        data->prev->link = data->link;
        data->link->prev = data->prev;
    }
    data->link = data->prev = NULL;
}
void cut_ack(AckQue* ack)
{
    if(ack==NULL) return;
    else if(ack==AQ_front)
    {
        AQ_front = AQ_front->link;
        if(AQ_front!=NULL) AQ_front->prev = NULL;//2개 이상
        else AQ_rear = NULL;//1개
    }
    else if(ack==AQ_rear)
    {
        //2개 이상
        AQ_rear = AQ_rear->prev;
        AQ_rear->link = NULL;
    }
    else
    {
        //3개 이상
        ack->prev->link = ack->link;
        ack->link->prev = ack->prev;
    }
    ack->link = ack->prev = NULL;
}
void print_performance_measure(void)
{
    double util;
    double m_delay;

    m_delay = t_delay/t_pknum;
    util = (t_pknum*t_pk)/cur_tm;//cur_tm 이 초기에 0에서 시작하도록 하였으므로 duration 과 cur_tm 값이 같다. -> 즉 simul_tm 이 곧 cur_tm.
    //m_delay = t_delay/N;
    //util = (t_pknum*t_pk)/simul_tm;
    
    printf("delay: %f\n", m_delay);//fprintf(file, "%f	", m_delay);
    printf("util: %f\n", util);//fprintf(file, " %f\n", util);
    
    //printf("Throughput : %f packets per unit time.\n", (float)t_pknum/cur_tm);
    //printf("Average Delay : %f unit time.\n", t_delay/t_pknum);
}

void show(void)
{
    printf("\n\n");
    DataQue *ptr = TransitQ_front;
    DataQue *wptr = WQ_front;
    AckQue *aptr = AQ_front;
    printf("T: ");
    while(ptr!=NULL)
    {
        printf("(%ld, %f), ", ptr->sn, ptr->t_out);
        ptr = ptr->link;
    }
    printf("\n");
    int cnt = 0;
    printf("W: ");
    while(wptr!=NULL && cnt++<10)//W가 너무 길어질 수 있어서 중간까지만 출력
    {
        printf("%ld, ", wptr->sn);
        wptr = wptr->link;
    }
    printf("\n");
    printf("A: ");
    while(aptr!=NULL)
    {
        printf("(%ld, %f), ", aptr->sn, aptr->ack_rtm);
        aptr = aptr->link;
    }
    printf("\n");
    
    printf("cur_time: %f\n", cur_tm);
}

void memory_dealloacation(void)
{
    if(TransitQ_front!=NULL)
    {
        DataQue *tmp;
        while(TransitQ_front!=NULL)
        {
            tmp = TransitQ_front;
            TransitQ_front = TransitQ_front->link;
            free(tmp);
            tmp = NULL;
        }
    }
    if(WQ_front!=NULL)
    {
        DataQue *tmp;
        while(WQ_front!=NULL)
        {
            tmp = WQ_front;
            WQ_front = WQ_front->link;
            free(tmp);
            tmp = NULL;
        }
    }
    if(AQ_front!=NULL)
    {
        AckQue *tmp;
        while(AQ_front!=NULL)
        {
            tmp = AQ_front;
            AQ_front = AQ_front->link;
            free(tmp);
            tmp = NULL;
        }
    }
}
void init(void)
{
    cur_tm=0;
    seq_n=0; transit_pknum = 0;
    next_acksn=0;
    t_pknum=0; t_delay=0;
}
void simul(void)
{
    WQ_front = WQ_rear = NULL;
    TransitQ_front = TransitQ_rear = NULL;
    AQ_front = AQ_rear = NULL;

    cur_tm = -log(random01())/lambda;//0초에서 시작해서 -log(random01())/lambda초에 최초 패킷도착.
    pk_gen(cur_tm);
    next_pk_gentm = cur_tm - log(random01())/lambda;

    while(t_pknum <= N)
    {
        if(SHOW==1) show();

        AckQue *tmp = AQ_front;
        while(tmp != NULL)
        {
            //process all ACK arrived before cur_tm
            //if ACK arrived as 1 3, not 1 2 3 -> 2's timeout
            //rtm 은 순서대로 담김.(오름차순)
            if(tmp->ack_rtm <= cur_tm)
                suc_transmission(tmp->sn);
            else break;
            tmp = tmp->link;
        }

        //tranQ element 각각에 대해 판단.
        if(TransitQ_front != NULL)
        {
            DataQue *tmp = TransitQ_front;
            if(tmp!=NULL)
                if(tmp->t_out <= cur_tm)
                {
                    if(SHOW==1) printf("\ntime out %ld\n", TransitQ_front->sn);
                    //tmp 만 재전송 위치(W)로 cut & paste
                    cut_data(tmp);
                    tmp->link = WQ_front;
                    if(WQ_front==NULL) WQ_rear = tmp;
                    WQ_front = tmp;

                    //W 의 front에 재전송 할 패킷 넣고, 바로 재전송. 단 pknum 은 증가X
                    transmit_pk();
                    receive_pk(TransitQ_rear->sn, TransitQ_rear->gentm);
                    //tmp = TransitQ_front;
                }
        }

        while(next_pk_gentm <= cur_tm)
        {
            pk_gen(next_pk_gentm);
            next_pk_gentm += -log(random01())/lambda;
        }
        if((WQ_front!=NULL) && (transit_pknum<W))
        {
            transmit_pk();
            transit_pknum++;
            receive_pk(TransitQ_rear->sn, TransitQ_rear->gentm);
        }

        cur_tm_update();
        
    }

    print_performance_measure();
    memory_dealloacation();
}