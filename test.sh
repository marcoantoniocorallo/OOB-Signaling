#!/bin/bash

#Lancia 8 server
echo "Supervisor e servers in esecuzione in background"
./supervisor 8 >& supervisor.txt &
sleep 2

#lancia 2 client al secondo
for (( i = 0; i < 20; i+=2 )); do
    var=$(expr $i + 1)
    ./client 5 8 20 >& client${i}.txt &
    ./client 5 8 20 >& client${var}.txt &
    echo "Client $i e $var in esecuzione"
    sleep 1
done

for (( i = 0; i < 6; ++i )); do
    pkill -INT supervisor
    echo "SIGINT inviato"
    sleep 10
done

pkill -INT supervisor
pkill -INT supervisor

./misura.sh supervisor.txt client0.txt client1.txt client2.txt              \
            client3.txt client4.txt client5.txt client6.txt client7.txt     \
            client8.txt client9.txt client10.txt client11.txt client12.txt  \
            client13.txt client14.txt client15.txt client16.txt client17.txt client18.txt client19.txt

#Elimina file di logs
rm supervisor.txt
rm REVERSE_SF.txt
for (( i = 0; i < 20; i++ )); do
    rm client${i}.txt
done

echo "Terminato e misurato."
