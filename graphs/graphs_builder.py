import pandas as pd
import matplotlib.pyplot as plt

case_names = ['argobots_tree_summary', 'argobots_tree_summary_small', 'argobots_simple_summary', 'argobots_simple_summary_small'] 
for case_name in case_names:
    df = pd.read_csv(case_name + '.csv')

    # Calculate speedup (acceleration) using the single-threaded execution as the baseline
    baseline_time = df[(df['Xstreams'] == 1) & (df['Threads'] == 1)]['Time(nanos)'].values[0]
    df['Speedup'] = baseline_time / df['Time(nanos)']

    # Plot mean execution time and speedup for each xstreams configuration
    for xstreams in df['Xstreams'].unique():
        subset = df[df['Xstreams'] == xstreams]
        
        # Plot mean execution time
        plt.figure(figsize=(10, 6))
        plt.plot(subset['Threads'], subset['Time(s)'], marker='o')
        plt.title(f'Mean Execution Time vs Threads (Xstreams={xstreams}) ({case_name})')
        plt.xlabel('Number of Threads')
        plt.ylabel('Mean Time (s)')
        plt.grid(True)
        plt.savefig(f'{case_name}/mean_execution_time_xstreams_{xstreams}.png')
        
        plt.close()

        # Plot mean execution time in nanosecods
        plt.figure(figsize=(10, 6))
        plt.plot(subset['Threads'], subset['Time(nanos)'] / 1e9, marker='o')
        plt.title(f'Mean Execution Time vs Threads (Xstreams={xstreams}) ({case_name})')
        plt.xlabel('Number of Threads')
        plt.ylabel('Mean Time (s)')
        plt.grid(True)
        plt.savefig(f'{case_name}/mean_execution_time_xstreams_{xstreams}_nanos.png')
        
        plt.close()
        
        # Plot speedup
        plt.figure(figsize=(10, 6))
        plt.plot(subset['Threads'], subset['Speedup'], marker='o')
        plt.title(f'Speedup vs Threads (Xstreams={xstreams}) ({case_name})')
        plt.xlabel('Number of Threads')
        plt.ylabel('Speedup')
        plt.grid(True)
        plt.savefig(f'{case_name}/speedup_xstreams_{xstreams}.png')
        
        plt.close()


# Also plot speedup of tree case vs simple case
df_tree = pd.read_csv('argobots_tree_summary.csv')
df_simple = pd.read_csv('argobots_simple_summary.csv')
for xstreams in df_tree['Xstreams'].unique():
    subset_tree = df_tree[df_tree['Xstreams'] == xstreams]
    subset_simple = df_simple[df_simple['Xstreams'] == xstreams]
    speedup = subset_simple['Time(nanos)'] / subset_tree['Time(nanos)']
    
    plt.figure(figsize=(10, 6))
    plt.plot(subset_tree['Threads'], speedup, marker='o')
    plt.title(f'Speedup of Tree Case vs Simple Case (Xstreams={xstreams})')
    plt.xlabel('Number of Threads')
    plt.ylabel('Speedup')
    plt.grid(True)
    plt.savefig(f'tree_vs_simple_speedup/tree_vs_simple_xstreams_{xstreams}.png')
    plt.close()

# and speedup of tree case vs simple case for small case
df_tree_small = pd.read_csv('argobots_tree_summary_small.csv')
df_simple_small = pd.read_csv('argobots_simple_summary_small.csv')
for xstreams in df_tree_small['Xstreams'].unique():
    subset_tree = df_tree_small[df_tree_small['Xstreams'] == xstreams]
    subset_simple = df_simple_small[df_simple_small['Xstreams'] == xstreams]
    speedup = subset_simple['Time(nanos)'] / subset_tree['Time(nanos)']
    
    plt.figure(figsize=(10, 6))
    plt.plot(subset_tree['Threads'], speedup, marker='o')
    plt.title(f'Speedup of Tree Case vs Simple Case (Small Case, Xstreams={xstreams})')
    plt.xlabel('Number of Threads')
    plt.ylabel('Speedup')
    plt.grid(True)
    plt.savefig(f'tree_vs_simple_speedup_small/tree_vs_simple_xstreams_{xstreams}.png')
    plt.close()