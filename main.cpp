#include "order.h"
#include "shipment_arrival.h"
#include "semaphore.cpp"
#include <sys/shm.h>
#include <iostream>
#include <time.h>
#include <memory>
#include <signal.h>

using namespace std;

const int NUMITEMS = 5;
const int OVERGRIPS = 0;
const int STRINGS = 1;
const int SHOES = 2;
const int BALLS = 3;
const int ACCESSORIES = 4;
const int NUMORDERS = 100 * 10; // Each of the 10 processes places 100 orders

enum inv{UPDATE_INV};
enum orders{PLACE_ORDER, FUFILL_ORDER};
SEMAPHORE inv_sem(1);
SEMAPHORE order_sem(2);
order *o;

void fufillOrder(order &o, int *inventory){
    inv_sem.P(UPDATE_INV);
    // First check if the entire order can be fufilled
    bool cannotFufill = false;
    for(int i = 0; i < NUMITEMS; i++){
        if(*(o.numItems + i)){ 
            if(*(inventory + i) == 0) { // We cannot fufill this item
                cannotFufill = true;
                break;
            }
        }
    }

    if(cannotFufill){
        inv_sem.V(UPDATE_INV);
        return;
    }
    else{ // Fufill the order
        for(int i = 0; i < NUMITEMS; i++){
            if(*(o.numItems + i)){ // If the order contains an item, deduct it
                *(inventory + i) -= 1;
            }
        }
        o.shipped = true;
        inv_sem.V(UPDATE_INV);

        // cout << "AFTER FUFILLMENT, In our inventory we have: " << endl;
        // for(int i = 0; i < NUMITEMS; i++){
        //     cout << "   " << i << ": " << *(inventory + i) << endl;
        // }
    }
    

}

void performRestock(int *inventory){
    int *recieved = shipment_arrival(new int[NUMITEMS]);
    
    inv_sem.P(UPDATE_INV);
    for(int i = 0; i < NUMITEMS; i++){ // "Restock"
        *(inventory + i) += *(recieved + i);
    }
    inv_sem.V(UPDATE_INV);

    // cout << "AFTER RESTOCK, In our inventory we have: " << endl;
    // for(int i = 0; i < NUMITEMS; i++){
    //     cout << "   " << i << ": " << *(inventory + i) << endl;
    // }
    // cout << "-----------------------------------" << endl;

}

// Random generates an "order" of items and returns it
unique_ptr<order> generateOrder(){
    unique_ptr<order> o(new order());
    int itemOrdered;

    o->PID = getpid();
    o->shipped = false;
    o->numItems[OVERGRIPS] = true; // Every order contains an overgrip
    for(int i = 1; i < NUMITEMS; i++){
        itemOrdered = rand() % 2;
        *(o->numItems + i) = (bool)itemOrdered;
    }

    return o;
}

void customer_proc(order *orders, int *order_index){
    int orderCount = 1;

    while(orderCount <= 100){ //Each customer places 100 orders
        order_sem.P(PLACE_ORDER);

        *(order_index) += 1; // Increment offset for placing orders at end of buffer
        *(orders + *(order_index)) = *(generateOrder());
        (orders + *(order_index))->orderNumber = orderCount;
        cout << "(Customer PID: " << (orders + *(order_index))->PID 
        << ") PLACED THEIR ORDER #" << orderCount << "/100\n";
        
        order_sem.V(FUFILL_ORDER);
        orderCount++;
    }
    cout << "(Customer PID: " << getpid() << ") HAS SHIPPED 100/100 ORDERS, Terminating . . .-----------------------------\n";
}

void fufillment_proc(order *orders, int *order_index, int *inventory){
    int fufillCount = 1;

    while(fufillCount <= NUMORDERS){
        order_sem.P(FUFILL_ORDER);
        o = (orders + *(order_index + 1));
        fufillOrder(*(o), inventory);

        if(o->shipped){
            *(order_index + 1) += 1; // Increment offset for next order to fufill
            cout << "(Customer PID: " << o->PID << ") ORDER #" << o->orderNumber << "/100 WAS FUFILLED\n";
            fufillCount++;
            cout << "------------------------------------------------\n";
            order_sem.V(PLACE_ORDER); // This customer is notified that it can now place new orders
        }
        order_sem.V(FUFILL_ORDER);
    }
}


void cleanup(int inv_id, int order_id, int index_id){
    shmctl(inv_id, IPC_RMID, NULL);
    shmctl(order_id, IPC_RMID, NULL);
    shmctl(index_id, IPC_RMID, NULL);

	inv_sem.remove();
    order_sem.remove();
}

int main(){
    int inv_id, order_id, index_id;
    int *inventory;
    int *order_index;
    order *orders;
    srand(time(NULL));

    // Initialize shared mem used between processes
    inv_id = shmget(IPC_PRIVATE, NUMITEMS*sizeof(int), PERMS);
    inventory = (int *)shmat(inv_id, 0, SHM_RND); // Used to update/get stock

    order_id = shmget(IPC_PRIVATE, NUMORDERS*sizeof(order), PERMS);
    orders = (order *)shmat(order_id, 0, SHM_RND); // Records each order and it's info

    index_id = shmget(IPC_PRIVATE, 2*sizeof(int), PERMS);
    order_index = (int *)shmat(index_id, 0, SHM_RND); // Used as offset for orders buffer 

    *(order_index) = -1; // offset for placing orders at the end of the buffer
    *(order_index + 1) = 0; // offset for next order to fufill
    order_sem.V(PLACE_ORDER);
    inv_sem.V(UPDATE_INV);


    for(int i = 0; i < NUMITEMS; i++){ // Initialize inventory to 0
        *(inventory + i) = 0; 
    } 

    if(fork()){
        pid_t restockPID = fork();
        if(restockPID){ // Parent fufillment process
            fufillment_proc(orders, order_index, inventory);
            kill(restockPID, SIGTERM);
            cout << "\nCleaning up and terminating . . .\n";
            cleanup(inv_id, order_id, index_id);
        }
        else{ // Shipment receiver process
            while(true){
                performRestock(inventory); // Continues restocking until signaled to terminate
            }
        }
    }
    
    else{ // Customer processes
        if(fork()){
            if(fork()){
                if(fork()){
                    if(fork()){
                        if(fork()){
                            if(fork()){
                                if(fork()){
                                    if(fork()){
                                        if(fork()){ // Customer #10
                                            customer_proc(orders, order_index); 
                                        }
                                        else { customer_proc(orders, order_index); } // Customer #9
                                    }
                                    else { customer_proc(orders, order_index);} // Customer #8
                                }
                                else { customer_proc(orders, order_index); } // Customer #7
                            }
                            else { customer_proc(orders, order_index); } // Customer #6
                        }
                        else { customer_proc(orders, order_index); } // Customer #5
                    }
                    else{ customer_proc(orders, order_index); } // Customer #4
                }
                else{ customer_proc(orders, order_index); } // Customer #3
            }
            else{ customer_proc(orders, order_index); } // Customer #2
        }
        else{ customer_proc(orders, order_index); } // Customer #1
    }
}