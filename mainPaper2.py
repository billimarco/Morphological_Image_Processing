import pandas as pd
import matplotlib.pyplot as plt
import os

# Parametri modificabili
dimensioni = ['400x400', '1600x1600', '6400x6400']
dischi = ['disk1', 'disk5']
versioni = ['V1', 'V2', 'V3']

metrica = '_Total'  # Cambia in '_Total' per plottare Erosion/Dilation Total altrimenti ''

# Colori univoci per ogni combinazione versione-operazione
tab10 = plt.get_cmap('tab10').colors
operazioni = ['Erosion', 'Dilation']
linee = [(v, op) for v in versioni for op in operazioni]
colori_linee = {f'{op}_{v}': tab10[i % len(tab10)] for i, (v, op) in enumerate(linee)}

# Crea la figura: righe = dischi, colonne = dimensioni
fig, axes = plt.subplots(len(dischi), len(dimensioni), figsize=(11.69, 8.27), sharex=True, sharey=False)

# Cicla su tutte le combinazioni
for row, disco in enumerate(dischi):
    for col, dimensione in enumerate(dimensioni):
        ax = axes[row, col] if len(dischi) > 1 else axes[col]

        for v in versioni:
            file_path = f'build/Release/results/{dimensione}_{disco}/csv_times_{v}_{dimensione}_{disco}.csv'
            df = pd.read_csv(file_path)
            for op in operazioni:
                col_key = f'{op[0]}{metrica}_Par'  # E_Mean o E_Total, ecc.
                label = f'{op} ({v})'
                color = colori_linee[f'{op}_{v}']
                ax.plot(df['Threads'], df[col_key], label=label, color=color, linestyle='-', marker='o')
                
        # Titoli e assi
        if row == 0:
            ax.set_title(f'{dimensione}')
        if col == 0:
            ax.set_ylabel(f'{disco}\nTime (s)')
        if row == len(dischi) - 1:
            ax.set_xlabel('Threads')

        ax.grid(True)
        ax.set_xticks(range(0, df['Threads'].max() + 1, 4))

# Legenda globale
handles, labels = axes[0, 0].get_legend_handles_labels() if len(dischi) > 1 else axes[0].get_legend_handles_labels()
fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 0.96), ncol=3, fontsize='medium')

# Titolo e layout
if metrica == '_Total':
    plt.suptitle(f'Confronto Erosion/Dilation Total - Versioni V1, V2, V3', fontsize=14)
else:
    plt.suptitle(f'Confronto Erosion/Dilation Mean - Versioni V1, V2, V3', fontsize=14)
plt.tight_layout(rect=[0, 0, 1, 0.93])
plt.show()

