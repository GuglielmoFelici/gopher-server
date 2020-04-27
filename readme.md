# Gopher Server

## Utilizzo del software

E' possibile compilare il progetto tramite make. La compilazione genera l'eseguibile linuxserver o,
nel caso in cui si esegua la build su Windows, il file winserver.exe e due binari aggiuntivi.
Viene effettuato il linking alla libreria pthread su GNU/Linux e alla libreria Winsock (Ws2_32) su Windows.
Il programma riconosce le seguenti opzioni da riga di comando:

```
-d DIR
    Utilizza la directory DIR come root del server.
-f FILE 
    Leggi le configurazioni da FILE. 
    Per informazioni sulla struttura del file, vedere la sezione 
    1.1 Lettura della configurazione.
-h
    Mostra la sintassi.
-l FILE
    Logga i trasferimenti sul file FILE. 
    Default: logfile.
-m 
    Utilizza un processo per ogni client. 
    Default: no.
-p PORT
    Metti il server in ascolto sulla porta PORT. 
    Default: 7070.
-v
    Imposta il livello di log informativi del server 
    (syslog su Linux, console su Windows). I livelli sono:
    0 - Non loggare nulla.
    1 - Logga gli errori.
    2 - Logga gli errori e i warning.
    3 - Logga gli errori, i warning e i messaggi di info.
    4 - Logga errori, warning, messaggi di info e messaggi di debug.
    Default: 2.
    ```
    
Le opzioni da riga di comando hanno precedenza su quelle scritte nel file di configurazione.\\
Se non viene specificata alcuna root, verrà usata la directory contenente l'eseguibile.

## Lettura della configurazione

Se viene utilizzato un file di configurazione con l'opzione -f, le impostazioni non specificate
da riga di comando vengono lette dal file dato in input. Questo deve essere una sequenza di righe del tipo CHIAVE = VALORE, 
dove sono accettate le seguenti coppie:

    - port = <numero di porta>
    - multiprocess = yes/no
    - verbosity = {1..4}
    - logfile = <path>
    - root = <path>
    
La lettura avviene all'avvio dell'applicazione e ogniqualvolta venga ricevuto il segnale 
SIGHUP (caso Linux) o CTRL_BREAK (caso Windows) durante il main loop del server; tuttavia, nel caso in cui si ricarichino le configurazioni mentre il server è in esecuzione,
non verranno aggiornati né il path di logging né la root directory.
