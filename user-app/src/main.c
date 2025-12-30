#include <stdio.h>
#include "forge_server.h"

int main() {
    // ১. সার্ভার তৈরি করুন (পোর্ট: ৮০৮০, ব্যাকলগ: ১০)
    // আমরা আগের কোডে 'ForgeServer' এবং 'create_forge_server' নাম দিয়েছিলাম
    
    ForgeServer app = create_forge_server(8080, 10);
    
    printf("ওয়েবসাইটটি দেখতে ব্রাউজারে যান: http://localhost:8080\n");

    // ২. সার্ভারটি রান বা স্টার্ট করুন
    launch_server(&app);

    return 0;
}
