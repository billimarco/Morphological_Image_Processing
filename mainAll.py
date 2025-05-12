import pandas as pd
import matplotlib.pyplot as plt
import os

# Parametri modificabili
dimensioni = ['400x400', '1600x1600', '6400x6400']  # Aggiungi tutte le dimensioni desiderate
dischi = ['disk1']  # Aggiungi tutti i dischi desiderati
versioni = ['V1', 'V2', 'V3']

# Usa la palette tab10
tab10 = plt.get_cmap('tab10').colors
categorie = ['E', 'D', 'O', 'C']
colori = {cat: tab10[i] for i, cat in enumerate(categorie)}

# Etichette personalizzabili per la legenda
labels_mean = ['Erosion (Mean)', 'Dilation (Mean)', 'Opening (Mean)', 'Closure (Mean)']
labels_total = ['Erosion (Total)', 'Dilation (Total)', 'Opening (Total)', 'Closure (Total)']

# Crea una figura con subplot per ogni combinazione di dimensione e disco
fig, axes = plt.subplots(len(dimensioni) * len(dischi), len(versioni), figsize=(8.27, 11.69), sharey=True)

# Cicla su ogni combinazione di dimensione e disco
for dim_idx, dimensione in enumerate(dimensioni):
    for disco_idx, disco in enumerate(dischi):
        base_path = f'build/Release/results/{dimensione}_{disco}/'
        versioni = ['V1', 'V2', 'V3']
        file_names = [f'csv_speedup_{v}_{dimensione}_{disco}.csv' for v in versioni]
        file_paths = [os.path.join(base_path, name) for name in file_names]

        # Titoli delle versioni
        titoli = [f'Speedup {v}' for v in versioni]
        
        for i, file in enumerate(file_paths):
            df = pd.read_csv(file)
            ax = axes[dim_idx * len(dischi) + disco_idx, i]  # Seleziona l'asse per categoria e combinazione di dimensione e disco

            for j, cat in enumerate(categorie):
                # Plot della media
                ax.plot(df['Threads'], df[f'{cat}_Mean'], label=labels_mean[j], color=colori[cat], linestyle='-', marker='o')
                # Plot del totale
                ax.plot(df['Threads'], df[f'{cat}_Total'], label=labels_total[j], color=colori[cat], linestyle='--', marker='s')

                # Impostazioni del grafico
                if dim_idx * len(versioni) + disco_idx == 0:  # Aggiungi il titolo per ogni colonna (solo per la prima riga)
                    ax.set_title(titoli[i])
                if i == 0:  # Aggiungi l'etichetta dell'asse y per la prima colonna
                    ax.set_ylabel('Speedup')
                if dim_idx * len(dischi) + disco_idx == len(dimensioni) * len(dischi) - 1:
                    ax.set_xlabel('Threads')

                ax.grid(True)
                ax.set_xticks(range(0, df['Threads'].max() + 1, 4))

# Legenda unica per tutta la figura (sotto i subplot)
handles, labels = axes[0, 0].get_legend_handles_labels()
fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 0.94), ncol=4, fontsize='medium')

# Titolo principale della figura
plt.suptitle('Confronto Speedup con disco di dimensione 1', fontsize=16)

# Ottimizza il layout per evitare sovrapposizioni
plt.tight_layout(rect=[0, 0, 1, 0.92])  # lascia più spazio sopra per la legenda
plt.show()
