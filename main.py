import pandas as pd
import matplotlib.pyplot as plt
import os

# Parametri modificabili
dimensione = '400x400'
disco = 'disk1'

# Percorso base dei file CSV
base_path = f'build/Release/results/{dimensione}_{disco}/'

# Nomi dei file CSV (sostituisci con i tuoi nomi reali)
versioni = ['V1', 'V2', 'V3']
file_names = [f'csv_speedup_{v}_{dimensione}_{disco}.csv' for v in versioni]
file_paths = [os.path.join(base_path, name) for name in file_names]
titoli = [f'Speedup {v}' for v in versioni]

# Usa la palette tab10
tab10 = plt.get_cmap('tab10').colors
categorie = ['E', 'D', 'O', 'C']
colori = {cat: tab10[i] for i, cat in enumerate(categorie)}

# Etichette personalizzabili per la legenda
labels_mean = ['Erosion (Mean)', 'Dilation (Mean)', 'Opening (Mean)', 'Closure (Mean)']  # Cambia le etichette per la media
labels_total = ['Erosion (Total)', 'Dilation (Total)', 'Opening (Total)', 'Closure (Total)']  # Cambia le etichette per il totale

# Crea la figura
fig, axes = plt.subplots(1, 3, figsize=(18, 6), sharey=True)

for i, file in enumerate(file_paths):
    df = pd.read_csv(file)
    ax = axes[i]

    for idx, cat in enumerate(categorie):
        # Plot della media
        ax.plot(df['Threads'], df[f'{cat}_Mean'], label=labels_mean[idx], color=colori[cat], linestyle='-', marker='o')
        # Plot del totale
        ax.plot(df['Threads'], df[f'{cat}_Total'], label=labels_total[idx], color=colori[cat], linestyle='--', marker='s')


    ax.set_title(titoli[i])
    ax.set_xlabel('Threads')
    ax.grid(True)
    if i == 0:
        ax.set_ylabel('Speedup')

# Legenda unica
handles, labels = axes[0].get_legend_handles_labels()
fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 0.94), ncol=4, fontsize='medium')


plt.suptitle('Confronto Speedup / Metriche Normalizzate su Tre Dataset', fontsize=16)
plt.tight_layout(rect=[0, 0, 1, 0.92])  # lascia più spazio in alto
plt.show()
