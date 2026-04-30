### 1. Problemi di rendering della mappa (Mappa Invisibile)

Il progetto utilizza le *Virtual Shadow Maps (VSM)* e le API *DirectX 12* (impostazioni di default di Unreal Engine 5). Su alcune configurazioni hardware o driver meno recenti, gli Actor (le tile) generati dinamicamente a runtime dal C++ potrebbero non essere renderizzati correttamente, risultando invisibili.

*   **Soluzione:** Se all'avvio la griglia di gioco non dovesse apparire, è sufficiente andare in `Project Settings -> Engine -> Rendering -> Shadows` e impostare il metodo delle ombre da *Virtual Shadow Maps* a **Shadow Maps**. In alternativa, è possibile forzare l'avvio in DirectX 11.

### 2. Stato di Implementazione dei Requisiti

1. **Il progetto compila correttamente, il codice è ben commentato e ben strutturato (polimorfismo ed ereditarietà):** Implementato (Ok).
2. **Griglia di gioco iniziale graficamente corretta e interamente visibile nello schermo:** Implementato (Ok).
3. **Meccanismo di posizionamento Unità di Gioco e Torri come da specifiche:** Implementato (Ok). 
4. **AI che utilizza algoritmo A* (movimento e attacco):** Implementato (Ok). 
5. **Il gioco funziona a turni e termina quando un giocatore vince:** Implementato (Ok). 
6. **Interfaccia grafica rappresentante lo stato corrente del gioco:** Implementato (Ok).
7. **Suggerimenti del range di movimento possibile per ciascuna unità:** Implementato (Ok). 
8. **Implementazione del meccanismo del danno da contrattacco:** Implementato (Ok). 
9. **Lista dello storico delle mosse eseguite:** Implementato (Ok). 
10. **AI che utilizza algoritmi euristici ottimizzati di movimento (diverso da A*):** Implementato (Ok).
