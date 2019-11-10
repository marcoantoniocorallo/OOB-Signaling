#!/bin/bash

CORRETTI=0
ERRATI=0

#Identifichiamo l'argomento contenente l'esito del/dei supervisor/Servers
for arg in $@ ; do
    read -r firstline < $arg
    case $firstline in
        ( "SERVER"* )  SERVER_FILE=$arg;;
    esac
done

#Invertiamo il contenuto del file per fermarci prima durante la scansione
tac $SERVER_FILE > REVERSE_SF.txt
RV="REVERSE_SF.txt"

#Per ogni client, memorizza valore SECRET
i=0
for file in $@ ; do
    DIFFERENCE="NULL"
    ESTIMATE="NULL"
    exec 3<$RV
    if [[ $file != $SERVER_FILE ]]; then
        read -r SECRET < $file

        #Ritagliamo solo il valore di SECRET
        ID=${SECRET/*"CLIENT"/}
        ID=${ID/" SECRET"*/}
        SECRET=${SECRET/*"SECRET"/}

        #Controlla valore stimato dal supervisor per quel client
        while read -u 3 line ; do
            case $line in
            ( *"${ID} BASED"* )
                ESTIMATE=${line/*"ESTIMATE "/}
                ESTIMATE=${ESTIMATE/" FOR"*/}
                break;;
            esac
        done

        #Incrementa statistica
        if [[ $ESTIMATE!="NULL" ]]; then
            DIFFERENCE=$(($SECRET - $ESTIMATE))
            DIFFERENCE=${DIFFERENCE/-/}          #Rimuovo eventuali segni di negatività
            if [[  $DIFFERENCE -le 25 ]] ; then
                 (( CORRETTI++ ))
            else (( ERRATI++ ))
                 err[$i]=$DIFFERENCE
                 (( i++ ))
            fi
        fi
    fi
done

#Calcolo errore medio
sum=0
for num in $err ; do
    sum=$(($sum + $num))
done

#Output statistiche
tot=$(($# - 1))
echo    Statistiche:
echo    SECRET corretti: ${CORRETTI}/$tot
echo    SECRET errati: ${ERRATI}/$tot

echo    "Errore medio: $(($sum / $ERRATI)) calcolato in proporzione ai valori errati"
echo    "Errore medio: $(($sum / $tot)) calcolato in proporzione ai valori totali"