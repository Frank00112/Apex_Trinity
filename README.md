### 1. Problemi di rendering della mappa (Mappa Invisibile)

Il progetto utilizza le *Virtual Shadow Maps (VSM)* e le API *DirectX 12* (impostazioni di default di Unreal Engine 5). Su alcune configurazioni hardware o driver meno recenti, gli Actor (le tile) generati dinamicamente a runtime dal C++ potrebbero non essere renderizzati correttamente, risultando invisibili.

*   **Soluzione:** Se all'avvio la griglia di gioco non dovesse apparire, è sufficiente andare in `Project Settings -> Engine -> Rendering -> Shadows` e impostare il metodo delle ombre da *Virtual Shadow Maps* a **Shadow Maps**. In alternativa, è possibile forzare l'avvio in DirectX 11.
