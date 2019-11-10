# Out-Of-Band Signaling
###### Progetto universitario per il corso di Sistemi Operativi e Laboratorio - Università di Pisa, a.a. 2018/2019

È richiesta la realizzazione di un sistema per l'out-of-band signaling. 
Si tratta di una tecnica di comunicazione in cui due entità si scambiano informazioni senza trasmettersele direttamente, ma utilizzando segnalazione collaterale: per esempio, il numero di errori “artificiali”, o la lunghezza di un pacchetto pieno di dati inutili, o anche il momento esatto delle comunicazioni.
In questo caso, si vuole realizzare un sistema client-server, in cui i client possiedono un codice segreto (secret) e intendono comunicarlo ad un server centrale, senza però trasmetterlo esplicitamente. 
Lo scopo è rendere difficile intercettare il secret a chi stia catturando i dati in transito.
Nel progetto saranno presenti n client, k server, ed 1 supervisor. 
Viene lanciato per primo il supervisor, con un parametro k che indica quanti server vogliamo attivare;
il supervisor provvederà dunque a creare i k server (processi distinti). 
Gli n client vengono invece lanciati indipendentemente, ciascuno in tempi diversi. 

È richiesta, poi, la realizzazione di uno script bash che, ricevuti come argomenti i nomi di un insieme di file
contenenti l'output di supervisor, server e client, ne legga e analizzi i contenuti e stampi delle statistiche su
quanti secret sono stati correttamente stimati dal supervisor (intendendo per stima corretta un secret stimato
con errore entro 25 unita rispetto al valore del secret vero) e sull'errore medio di stima.

Il funzionamento di ciascuno dei componenti è descritto nel testo del progetto, mentre i dettagli implementativi sono discussi all'interno della relazione.
Il sistema Supervisor+Server è stato testato anche con clients di terze parti e non sono stati riscontrati problemi o anomalie.
#### Nota: L'intero progetto è stato sviluppato e distribuito riservando particolare interesse al tempo di consegna.
