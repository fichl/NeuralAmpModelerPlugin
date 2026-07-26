import os
import math
import struct
import subprocess
import csv

# We reuse functions from run_null_tests.py
from run_null_tests import load_wav_manual, run_null_test_on_files

def main():
    # Grid search parameters
    taps_list = [33, 65, 129, 257, 513, 1025]
    beta_list = [6.0, 8.0, 10.0, 12.0, 14.0]
    cutoff_list = [0.50, 0.60, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95]
    
    input_wav = "NeuralAmpModelerCore/example_audio/input.wav"
    output_wav = "opt_temp.wav"
    exe_path = "D:\\NAM-OS\\NAM-Oversampler\\AudioDSPTools\\build_test\\tools\\Release\\test_oversampler.exe"
    
    results = []
    
    total_runs = len(taps_list) * len(beta_list) * len(cutoff_list)
    print(f"Starting grid search over {total_runs} combinations...")
    
    run_idx = 0
    for taps in taps_list:
        for beta in beta_list:
            for cutoff in cutoff_list:
                run_idx += 1
                
                # Formula for expected group delay of oversampler in test_oversampler
                # LinearCascadedFIRLong has inner taps = 73
                # delay = (taps - 1)/2 + (inner_taps - 1)*7/16
                # (73 - 1) * 7 / 16 = 72 * 7 / 16 = 31.5
                exp_delay = (taps - 1) / 2.0 + 31.5
                
                # Execute C++ oversampler
                # Arguments: <input.wav> <output.wav> <taps> <beta> <cutoff_bias> <phase_mode>
                # phase_mode: 2 = LinearCascadedFIRLong
                cmd = [
                    exe_path,
                    input_wav,
                    output_wav,
                    str(taps),
                    str(beta),
                    str(cutoff),
                    "2"
                ]
                
                # Suppress stdout/stderr
                try:
                    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
                except subprocess.CalledProcessError as e:
                    print(f"Error running oversampler command for taps={taps}, beta={beta}, cutoff={cutoff}: {e}")
                    continue
                
                # Run the null test on the output wav
                try:
                    res = run_null_test_on_files(input_wav, output_wav, exp_delay)
                    res["taps"] = taps
                    res["beta"] = beta
                    res["cutoff"] = cutoff
                    results.append(res)
                    
                    if run_idx % 20 == 0 or run_idx == total_runs:
                        print(f"Progress: {run_idx}/{total_runs} - taps={taps}, beta={beta:.1f}, cutoff={cutoff:.2f} -> LUFS Null={res['null_lufs']:.2f} dB, RMS Null={res['null_rms']:.2f} dB, ESR={res['esr']:.8f}")
                except Exception as e:
                    print(f"Error processing metrics for taps={taps}, beta={beta}, cutoff={cutoff}: {e}")
                    continue
                    
    # Clean up temp file
    if os.path.exists(output_wav):
        os.remove(output_wav)
        
    # Write to CSV
    csv_file = "grid_search_results.csv"
    with open(csv_file, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["taps", "beta", "cutoff", "delay", "gain_db", "null_lufs", "null_rms", "null_peak", "esr", "mae", "mse", "mrstft"])
        writer.writeheader()
        for r in results:
            writer.writerow(r)
            
    print(f"\nSaved all results to {csv_file}")
    
    # Analyze best configurations
    print("\n=== TOP RESULTS BY METRICS ===")
    
    # We want to find the configuration that minimizes ESR/RMS/Peak error.
    # Note: null depth is negative, so "best" means most negative (lowest value).
    # ESR is positive, so "best" means closest to 0 (lowest value).
    # MRSTFT is positive, so "best" means closest to 0 (lowest value).
    
    # Best overall ESR
    best_esr = min(results, key=lambda x: x["esr"])
    print(f"Best overall ESR ({best_esr['esr']:.10f}): taps={best_esr['taps']}, beta={best_esr['beta']:.1f}, cutoff={best_esr['cutoff']:.2f} (RMS Null={best_esr['null_rms']:.2f} dB, Peak Null={best_esr['null_peak']:.2f} dB, MRSTFT={best_esr['mrstft']:.8f})")
    
    # Best overall MRSTFT
    best_mrstft = min(results, key=lambda x: x["mrstft"])
    print(f"Best overall MRSTFT ({best_mrstft['mrstft']:.10f}): taps={best_mrstft['taps']}, beta={best_mrstft['beta']:.1f}, cutoff={best_mrstft['cutoff']:.2f} (RMS Null={best_mrstft['null_rms']:.2f} dB, ESR={best_mrstft['esr']:.8f})")
    
    # Best overall Peak Null
    best_peak = min(results, key=lambda x: x["null_peak"])
    print(f"Best overall Peak Null ({best_peak['null_peak']:.2f} dB): taps={best_peak['taps']}, beta={best_peak['beta']:.1f}, cutoff={best_peak['cutoff']:.2f} (RMS Null={best_peak['null_rms']:.2f} dB, ESR={best_peak['esr']:.8f})")

    # Best overall RMS Null
    best_rms = min(results, key=lambda x: x["null_rms"])
    print(f"Best overall RMS Null ({best_rms['null_rms']:.2f} dB): taps={best_rms['taps']}, beta={best_rms['beta']:.1f}, cutoff={best_rms['cutoff']:.2f} (ESR={best_rms['esr']:.8f})")
    
    # Group by tap count to find optimal parameters per tap count
    print("\n=== OPTIMAL PARAMETERS PER TAP COUNT ===")
    for taps in taps_list:
        taps_results = [r for r in results if r["taps"] == taps]
        if not taps_results:
            continue
        # Find best by ESR
        best_taps_esr = min(taps_results, key=lambda x: x["esr"])
        # Find best by MRSTFT
        best_taps_mrstft = min(taps_results, key=lambda x: x["mrstft"])
        # Find best by Peak
        best_taps_peak = min(taps_results, key=lambda x: x["null_peak"])
        # Find best by RMS
        best_taps_rms = min(taps_results, key=lambda x: x["null_rms"])
        
        print(f"Taps = {taps:4d}:")
        print(f"  - Best ESR      ({best_taps_esr['esr']:.10f}): beta={best_taps_esr['beta']:.1f}, cutoff={best_taps_esr['cutoff']:.2f} | RMS Null={best_taps_esr['null_rms']:.2f} dB")
        print(f"  - Best RMS Null ({best_taps_rms['null_rms']:.2f} dB): beta={best_taps_rms['beta']:.1f}, cutoff={best_taps_rms['cutoff']:.2f} | ESR={best_taps_rms['esr']:.10f}")
        print(f"  - Best Peak Null({best_taps_peak['null_peak']:.2f} dB): beta={best_taps_peak['beta']:.1f}, cutoff={best_taps_peak['cutoff']:.2f} | ESR={best_taps_peak['esr']:.10f}")
        print(f"  - Best MRSTFT   ({best_taps_mrstft['mrstft']:.10f}): beta={best_taps_mrstft['beta']:.1f}, cutoff={best_taps_mrstft['cutoff']:.2f}")

if __name__ == "__main__":
    main()
