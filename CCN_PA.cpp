#include <iostream>
#include <random>
#include <chrono>
#include <string>
#include <thread>
#include <cstdlib>
#include <ctime>
using std::cout;
using std::cin;
using std::string;

//Smaller helper fucntion to animate a growing arrow which progresses while sleeping for propagation delay
static void animateDelay(int propDelay) {
    
    if (propDelay <= 0) {
        return;
    }

    const int msPerDash = 100; // Smaller number -> more dashes
    int steps = propDelay / msPerDash;
    
    if (steps < 1) steps = 1;
    
    int interval = propDelay / steps;
    
    if (interval < 1) interval = 1;

    // append a space so the animation starts after the current prompt
    cout.flush();
    cout << ' ';
    cout.flush();

    bool first = true;
    for (int i = 0; i < steps; ++i) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        
        if (first) {
            // first step: print a dash and the arrowhead
            cout << '-' << '>';
            first = false;
        } 
        
        else {
            // Move back over the '>' then insert a dash
            // before printing '>' again so the arrowhead stays at the end
            cout << '\b' << '-' << '>';
        }
        cout.flush();
    }

    // leave a space so the following status text doesn't run into the arrow
    cout << ' ';
    cout.flush();
    cout<<"\n\n";
}

//Packet object class   
class packet {
    public:
        int data;
        int seqNum;
        bool ack;
        bool drop;
        std::chrono::steady_clock::time_point sendTime;
        std::chrono::steady_clock::time_point ackTime;
        bool timeOut;
};

//Sender object class
class sender {
        int SeqNum;
        int windowSize;

public:
        packet *packets;
        std::chrono::milliseconds expectedDelay;

        //Packet array constructor
        void setup(int SeqNum, int windowSize, int numPackets, std::chrono::milliseconds expectedDelay) {
            this->SeqNum = SeqNum;
            this->windowSize = windowSize;
            this->expectedDelay = expectedDelay;
            packets = new packet[numPackets];

            //Packet initialization
            for(int i = 0; i < numPackets; i++) {
                packets[i].seqNum = i;
                packets[i].ack = false;
                packets[i].data = rand() % 1000; //Random data between 0 and 999
                packets[i].drop = false;
                packets[i].timeOut = false; // initialize to not-timeout by default
                packets[i].sendTime = std::chrono::steady_clock::time_point();
                packets[i].ackTime = std::chrono::steady_clock::time_point();
            }
        }

        // Function that checks for retransmission.
        // Returns an array of packets that were not acknowledged or were lost.
        // Must delete[] the returned array when done.
        packet *retransmit(int numPackets, packet packets[], int &retransmitCount) {
            
            // Reset count
            retransmitCount = 0;

            //In case of no packets
            if (numPackets <= 0 || packets == nullptr) {
                return nullptr;
            }

            // Count how many need retransmission
            for (int i = 0; i < numPackets; ++i) {
                
                if (packets[i].drop || packets[i].timeOut) {
                    ++retransmitCount;
                }
            }

            //In case no packets need retransmission
            if (retransmitCount == 0) {
                return nullptr;
            }

            // Allocate array for retransmission
            packet *toRetransmit = new packet[retransmitCount];
            int j = 0;
            
            //Recount from the original array,
            //marking packets for retransmission
            //Fill the retransmission array
            for (int i = 0; i < numPackets; ++i) {
                if (packets[i].drop || packets[i].timeOut) {
                    
                    toRetransmit[j++] = packets[i];
                }
            }
            return toRetransmit;
        }

};

//Receiver object class
class receiver { 
    int expectedSeqNum;
    int windowSize;
    std::chrono::steady_clock::time_point arrTime;
    

    public:
    packet *buffer;
        
    //Reciever's buffer array constructor
        void setup(int expectedSeqNum, int numPackets, int windowSize) {
            this->expectedSeqNum = expectedSeqNum;
            this->windowSize = windowSize;
            buffer = new packet[numPackets];

            //Buffer initialization
            for(int i = 0; i < numPackets; i++) {
                buffer[i].seqNum = i;
                buffer[i].ack = false;
                buffer[i].data = -1; //-1 indicates empty buffer slot
            }
        }
};


int main() {
    //Initialize all objects and variables
    int windowSize = 0, numPackets = 0, propDelay = 0, pLost = 0;
    int totalSent = 0, totalRecieved = 0, totalRetrans = 0;
    char choice;
    bool loop = true;
    sender theSender;
    receiver theReceiver; 
    
    // Seed the C RNG so calls to rand() return different values each run
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    //Then, allow user input for custom parameters
    while(loop) {

        cout<<"Would you like to set custom parameters? If no, default parameters will be used. (y/n): ";
        cin>>choice;
        cout<<"\n";

        if (choice == 'y' || choice == 'Y') {
            cout<<"Enter window size: ";
            cin>>windowSize;
            cout<<"Enter number of packets to send: ";
            cin>>numPackets;
            cout<<"Enter propagation delay (in milliseconds): ";
            cin>>propDelay;
            cout<<"\n";
            loop = false;
        }

        else if (choice == 'n' || choice == 'N') {
            cout<<"Using default parameters:\n";
            cout<<"Window Size = 2\n";
            cout<<"Number of Packets = 10\n";
            cout<<"Propagation Delay = 5 seconds\n\n";

            windowSize = 2;
            numPackets = 10;
            propDelay = 5000; //in milliseconds
            loop = false;
        }

        else {
            cout<<"Invalid input. Try again.\n\n";
        }
    }

    //Initialize sender with parameters
    //cout<<"Preparing sender... \n\n";
  
    //Sending input data to sender setup function
    theSender.setup(0, windowSize, numPackets, std::chrono::milliseconds(propDelay));

    //cout<<"Sender ready! \n\n";

    //cout<<"Preparing receiver... \n\n";

    theReceiver.setup(0, numPackets, windowSize);

    //cout<<"Receiver ready! \n\n";

    //-------------------------------------------------------------------

    //Need to set up for packet losses based on input.
    bool manualLoss = false;
    bool loop2 = true;
    loop = true; // ensure the packet-loss menu runs (reset after parameter selection)
    while (loop) {
        
        cout<<"Would you like to manually simulate packet loss? If no, packet loss will be randomized. (y/n): ";
        cin>>choice;
        cout<<"\n";

        if (choice == 'y' || choice == 'Y') {
            manualLoss = true;

            //This loop makes sure the user puts in a valid input for number of packets to lose
            //It'll keep asking until it gets a valid integer input
            while(loop2) {
                cout<<"How many packets would you like to lose? (0 to "<<numPackets<<"): ";
                cin>>pLost; 

                //Checks for invalid input
                //Clears cin and discards invalid input
                if(cin.fail()) {
                    cout<<"\nInvalid input. Try again.\n\n";
                    cin.clear(); // clear error flag
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                    continue;
                }

                //If the user selects 0 packets to lose, exit both loops
                else if(pLost == 0) {
                    cout<<"\nNo packets will be lost.\n\n";
                    loop = false;
                    loop2 = false;
                }

                //Checks if the input is in range
                else if(pLost < 0 || pLost > numPackets) {
                    cout<<"\nInvalid number of packets to lose. Try again.\n\n";
                }

                //Reports how many packets will be lost and exits loop
                else {
                    //cout<<"\n"<<pLost<<" packets will be lost.\n\n";
                    loop2 = false;
                }
            }

            cout<<"\nSpecify which packets you would like to lose by sequence number (0 to "<<numPackets - 1<<"):\n";
            int seqToLose = 0;
            cout<<"\n";

            for (int i = 0; i < pLost; i++) {
                
                cout<<"Packet: ";
                cin>>seqToLose;

               //First, see if input is not an integer
                if (cin.fail()) {
                    cout<<"Invalid input. Try again.\n";
                    cin.clear(); // clear error flag
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                    i--; // repeat this iteration
                    continue;
                }

                //Then, make sure input is in range
                else if (seqToLose < 0 || seqToLose >= numPackets) {
                    cout <<"Invalid sequence number. Try again.\n";
                    i--; // repeat this iteration
                    continue;
                }

                //Next, make sure packet isn't already marked to be lost
                else if (theSender.packets[seqToLose].drop) {
                    cout << "Packet " << seqToLose << " is already marked to be lost. Try again.\n";
                    i--; // repeat this iteration
                    continue;
                }

                // valid input AND in range AND unmarked -> mark packet to be dropped later
                else {
                    theSender.packets[seqToLose].drop = true;
                    //cout << "Packet " << seqToLose << " will be lost.\n";
                }

            }
            // finished manual selection; exit menu
            loop = false;
        }

        else if (choice == 'n' || choice == 'N') {
            cout<<"Randomizing packet loss...\n\n";
            pLost = rand() % (numPackets + 1); //Random number between 0 and numPackets
            
            //Randomly selects which packets to lose by sequence number
            for (int i = 0; i < pLost; i++) {
                
                int seqToLose = rand() % numPackets; //Random sequence number between 0 and numPackets - 1

                if(theSender.packets[seqToLose].drop) {
                    i--; // packet already marked to drop; repeat this iteration
                    continue;
                }

                theSender.packets[seqToLose].drop = true;

                //Need to store these somewhere to reference later
            }

            cout<<pLost<<" packets will be lost.\n";
            loop = false;
        }

        else {
            cin.clear(); // clear error flag
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
            cout<<"Invalid input. Try again.\n\n";
        }
    }

    //-------------------------------------------------------------------

    //Need to set up for ack losses based on input.
    
    //Reset loop variables
    loop = true;
    loop2 = true;
    bool manualAckLoss = false;
    int noAck = 0;
    while (loop) {

        //if all packets are lost, no point in asking about ack loss
        if(pLost == numPackets) {
            cout<<"All packets are lost, skipping acknowledgement loss setup.\n\n";
            break;
        }
        
        cout<<"\nWould you like to manually simulate acknowledgement loss? If no, acknowledgement loss will be randomized. (y/n): ";
        cin>>choice;
        cout<<"\n";

        if (choice == 'y' || choice == 'Y') {
            manualAckLoss = true;

            //This loop makes sure the user puts in a valid input for number of packets to lose
            //It'll keep asking until it gets a valid integer input
            while(loop2) {
                cout<<"How many packet acknowledgements would you like to lose? (0 to "<<numPackets<<"): ";
                cin>>noAck; 

                //Checks for invalid input
                //Clears cin and discards invalid input
                if(cin.fail()) {
                    cout<<"\nInvalid input. Try again.\n\n";
                    cin.clear(); // clear error flag
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                    continue;
                }

                else if (noAck == 0) {
                    cout<<"\nNo acknowledgements will be lost.\n\n";
                    loop = false;
                    loop2 = false;
                }

                else if(noAck < 0 || noAck > numPackets) {
                    cout<<"\nInvalid number of acknowledgements to lose. Try again.\n\n";
                }

                else {
                    //cout<<"\n"<<noAck<<" acknowledgements will be lost.\n\n";
                    loop2 = false;
                }
            }

            cout<<"\nSpecify which packets you would like to lose acknowledgement for by sequence number (0 to "<<numPackets - 1<<"):\n";
            int seqToLose = 0;
            cout<<"\n";

            for (int i = 0; i < noAck; i++) {
                
                cout<<"Packet: ";
                cin>>seqToLose;

               //First, see if input is not an integer
                if (cin.fail()) {
                    cout<<"Invalid input. Try again.\n";
                    cin.clear(); // clear error flag
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                    i--; // repeat this iteration
                    continue;
                }

                //Then, make sure input is in range
                else if (seqToLose < 0 || seqToLose >= numPackets) {
                    cout << "Invalid sequence number. Try again.\n";
                    i--; // repeat this iteration
                    continue;
                }

                //Make sure packet isn't already marked to be lost
                else if(theSender.packets[seqToLose].drop) {
                    cout << "Packet " << seqToLose << " is already marked to be lost. Try again.\n";
                    i--; // repeat this iteration
                    continue;
                }

                //Make sure packet isn't already marked to lose acknowledgement
                else if(theSender.packets[seqToLose].timeOut) {
                    cout << "Packet " << seqToLose << " is already marked to lose acknowledgement. Try again.\n";
                    i--; // repeat this iteration
                    continue;
                }

                // valid input AND in range -> mark packet to be dropped later
                else {
                    theSender.packets[seqToLose].timeOut = true;
                    //cout << "Packet " << seqToLose << " will not be acknowledged.\n";
                }
            }
            // finished manual selection; exit menu
            loop = false;
        }

        else if (choice == 'n' || choice == 'N') {
            cout<<"Randomizing acknowledgement loss...\n\n";
           
            noAck = rand() % (numPackets + 1); //Random number between 0 and numPackets

            // mark first noAck eligible packets
            int marked = 0;
            for (int idx = 0; idx < numPackets && marked < noAck; ++idx) {
                if (theSender.packets[idx].drop || theSender.packets[idx].timeOut) continue;
                
                theSender.packets[idx].timeOut = true;
                ++marked;
            }

            cout << marked << " will not be acknowledged.\n";
            loop = false;
            
        }

        else {
            cin.clear(); // clear error flag
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
            cout<<"Invalid input. Try again.\n\n";
        }
    }

    cout<<"\nSimulation parameters set!\n\n"; 
    
    //Display which packets are marked to be lost by user
    if(manualLoss) {
        cout<<"The following packets are marked to be lost:\n";
        for (int i = 0; i < numPackets; i++) {
            if (theSender.packets[i].drop) {
                cout<<"Packet "<<theSender.packets[i].seqNum<<"\n";
            }
        }
    }

    //Display which packets are marked to lose acknowledgement by user
    if(manualAckLoss) {
        cout<<"\nThe following packets are marked to lose acknowledgement:\n";
        for (int i = 0; i < numPackets; i++) {
            if (theSender.packets[i].timeOut) {
                cout<<"Packet "<<theSender.packets[i].seqNum<<"\n";
            }
        }
    }
    
    cout<<"\nStarting simulation...\n\n";

    

    //-------------------------------------------------------------------

    cout<<"\n----------------- Initial Transmission Phase ----------------\n\n";

    cout<<"\n\n";

    //Make sure the "timer" starts AFTER all other initializations
    auto start = std::chrono::steady_clock::now();
    //cout << "Timer starting now!\n\n";

    if (numPackets == 0) {
        cout << "No packets to send. Exiting simulation.\n";
        return 0;
    }

    //Initial transmission phase
    else{
        
        int sentInWindow = 0;
        for (int i = 0; i < numPackets; i++) {
            theSender.packets[i].sendTime = std::chrono::steady_clock::now();
            // count this transmission attempt
            ++totalSent;

            // Handle packet outcome and print status first
            if (theSender.packets[i].drop) {
                cout << "Packet " << theSender.packets[i].seqNum << " was dropped or lost during transmission, and not received by the receiver.\n\n";
            }
            else if (theSender.packets[i].timeOut) {
                cout << "Acknowledgement for packet " << theSender.packets[i].seqNum << " was not received.\n\n";
            }
            else {
                // Simulate packet arrival at receiver
                theReceiver.buffer[i] = theSender.packets[i];
                auto now = std::chrono::steady_clock::now();

                // record the arrival/ack times consistently on both sides
                theReceiver.buffer[i].ackTime = now;
                theSender.packets[i].ackTime = now;
                 theReceiver.buffer[i].ack = true;
                 cout << "Packet " << theSender.packets[i].seqNum << " received by receiver @ "
                     << std::chrono::duration<double>(now - start).count() << " seconds.\n";

                 cout << "\nAcknowledgement for packet " << theReceiver.buffer[i].seqNum << " sent back to the sender.\n\n";

                 // count this successful reception
                 ++totalRecieved;
            }

            // After processing the packet, count it toward the current window
            ++sentInWindow;

            // Once there's a full window, and its not the last packet, show propagation delay
            if (windowSize > 0 && sentInWindow >= windowSize && i != numPackets - 1) {
                cout << "Sending packets... ";
                animateDelay(propDelay);
                sentInWindow = 0;
            }
        }

        //-------------------------------------------------------------------
        
        cout<<"\n\n";

        cout<<"----------------- Buffer Statistics ----------------\n\n";

        cout<<"\n\n";

        //Reads the receiver buffer packets and prints acknowledgements
        cout<<"Current status of reciever buffer:\n\n";
        for (int i = 0; i < numPackets; i++) {
            
            if (theReceiver.buffer[i].ack) {
                cout << "Packet " << theReceiver.buffer[i].seqNum << " : Acknowledged\n\n";
            }
            
            else {
                cout << "Packet " << theReceiver.buffer[i].seqNum << " : Not Acknowledged\n\n";
            }
        }

        //-------------------------------------------------------------------

        //Now, retransmission of all lost and unacknowledged packets

        cout<<"\n\n";

        cout<<"----------------- Retransmission Phase ----------------\n\n";

        cout<<"\n\n";

        cout << "Retransmitting all lost and unacknowledged packets...\n\n";

        int retransmitCount = 0;
        packet *retransmitArr = theSender.retransmit(numPackets, theSender.packets, retransmitCount);

        // Reset sentInWindow for retransmission phase
        sentInWindow = 0;

        if (retransmitArr == nullptr || retransmitCount == 0) {
            cout << "No packets to retransmit. Exiting simulation.\n\n";
            return 0;
        } 
        
        else {
            for (int i = 0; i < retransmitCount; ++i) {
                retransmitArr[i].sendTime = std::chrono::steady_clock::now();

                // count this retransmission attempt
                ++totalSent;
                ++totalRetrans;

                // Reset loss flags on the retransmitted copy
                if (retransmitArr[i].drop) {
                    retransmitArr[i].drop = false;
                }

                // Simulate packet arrival at receiver using the packet's seqNum
                int seq = retransmitArr[i].seqNum;
                theReceiver.buffer[seq] = retransmitArr[i];
                auto now = std::chrono::steady_clock::now();

                theReceiver.buffer[seq].ackTime = now;
                retransmitArr[i].ackTime = now;

                theReceiver.buffer[seq].ack = true;

                // count this successful retransmission reception
                ++totalRecieved;

                // Update the sender's packet record so it is no longer
                // selected for retransmission in future passes.
                if (seq >= 0 && seq < numPackets) {
                    theSender.packets[seq].drop = false;
                    theSender.packets[seq].timeOut = false;
                    theSender.packets[seq].ack = true;
                    theSender.packets[seq].ackTime = now;
                }

                cout << "Packet " << seq << " received by receiver @ "
                     << std::chrono::duration<double>(now - start).count() << " seconds.\n\n";

                // After processing the packet, count it toward the current window
                ++sentInWindow;

                // If we've sent a full window (and it's not the very last retransmit), show propagation delay
                if (windowSize > 0 && sentInWindow >= windowSize && i != retransmitCount - 1) {
                    cout << "Sending packets... ";
                    animateDelay(propDelay);
                    sentInWindow = 0;
                }
            }

            delete[] retransmitArr;
        }

        cout << "All retransmissions complete.\n\n";

        cout<<"\n\n";

        cout<< "----------------- Send Off Phase ----------------\n\n";

        cout<<"\n\n";

        //Reset sentInWindow for final send-off phase
        sentInWindow = 0;
        for (int i = 0; i < numPackets; i++) {

            theReceiver.buffer[i].sendTime = std::chrono::steady_clock::now();
            // count this transmission attempt
            ++totalSent;

            //"Send" the packets to the application layer
            cout << "Packet " << theReceiver.buffer[i].seqNum << " sent to application layer @ "
                 << std::chrono::duration<double>(theReceiver.buffer[i].sendTime - start).count() << " seconds.\n\n";

            // After processing the packet, count it toward the current window
            ++sentInWindow;

            // If we've sent a full window (and it's not the very last packet), show propagation delay
            if (windowSize > 0 && sentInWindow >= windowSize && i != numPackets - 1) {
                cout << "Sending packets... ";
                animateDelay(propDelay);
                sentInWindow = 0;
            }

        }
    }

        cout << "Simulation complete!\n\n";

        cout<<"----------------- Statistics ----------------\n\n";

        // Print summary statistics
        cout << "  Total transmission attempts: " << totalSent << "\n";
        cout << "  Total successful receptions: " << totalRecieved << "\n";
        cout << "  Total retransmissions: " << totalRetrans << "\n\n";

        cout << "Exiting now.\n";   

    return 0;
}