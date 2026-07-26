import math
import struct
import subprocess
import os

def load_wav_manual(filename):
    with open(filename, 'rb') as f:
        riff_header = f.read(12)
        if len(riff_header) < 12 or riff_header[0:4] != b'RIFF' or riff_header[8:12] != b'WAVE':
            raise ValueError('Not a valid WAVE file')
        sample_rate = 0
        num_channels = 0
        bits_per_sample = 0
        audio_format = 0
        samples = []
        while True:
            chunk_header = f.read(8)
            if len(chunk_header) < 8:
                break
            chunk_id = chunk_header[0:4]
            chunk_size = struct.unpack('<I', chunk_header[4:8])[0]
            chunk_data = f.read(chunk_size)
            if chunk_id == b'fmt ':
                audio_format, num_channels, sample_rate, byte_rate, block_align, bits_per_sample = struct.unpack('<HHIIHH', chunk_data[:16])
            elif chunk_id == b'data':
                if audio_format == 3:
                    if bits_per_sample == 32:
                        n_samples = chunk_size // 4
                        samples = list(struct.unpack(f'<{n_samples}f', chunk_data[:n_samples*4]))
                    elif bits_per_sample == 64:
                        n_samples = chunk_size // 8
                        samples = list(struct.unpack(f'<{n_samples}d', chunk_data[:n_samples*8]))
                elif audio_format == 1:
                    if bits_per_sample == 16:
                        n_samples = chunk_size // 2
                        samples = [x / 32768.0 for x in struct.unpack(f'<{n_samples}h', chunk_data[:n_samples*2])]
                    elif bits_per_sample == 24:
                        n_samples = chunk_size // 3
                        samples = []
                        for i in range(n_samples):
                            b = chunk_data[i*3:i*3+3]
                            val = b[0] | (b[1] << 8) | (b[2] << 16)
                            if val & 0x800000:
                                val -= 0x1000000
                            samples.append(val / 8388608.0)
                    elif bits_per_sample == 32:
                        n_samples = chunk_size // 4
                        samples = [x / 2147483648.0 for x in struct.unpack(f'<{n_samples}i', chunk_data[:n_samples*4])]
            if chunk_size % 2 == 1:
                f.read(1)
        return samples, sample_rate, num_channels

def F(e):
    if abs(e) < 1e-12:
        return 1.0
    return math.sin(math.pi * e) / (math.pi * e)

def E(e):
    t = int(math.floor(e))
    l = e - t
    n = [0.0] * 64
    a = 0.0
    for i in range(64):
        x = i - 31
        r = 2.0 * math.pi * i / 63.0
        o = 0.35875 - 0.48829 * math.cos(r) + 0.14128 * math.cos(2.0 * r) - 0.01168 * math.cos(3.0 * r)
        n[i] = F(l - x) * o
        a += n[i]
    for i in range(64):
        n[i] /= a
    return {"base": t, "h": n}

def C(e, t, l, n=1, a=0, r=1e99):
    o = E(l)
    i_len = len(e[0])
    f_len = len(t[0])
    h = len(e)
    s = int(max(a, 0, 31 - o["base"]))
    c = int(min(r, i_len, f_len - o["base"] - 32))
    d, m, g = 0.0, 0.0, 0.0
    u, M, p = 0.0, 0.0, 0.0
    y = 0
    if c <= s:
        return {"q": 1.0, "g": 0.0}
    for l_idx in range(s, c, n):
        for channel in range(h):
            a_val = 0.0
            r_idx = l_idx + o["base"] - 31
            i_arr = t[channel]
            for filter_idx in range(64):
                a_val += i_arr[r_idx + filter_idx] * o["h"][filter_idx]
            f_val = e[channel][l_idx]
            
            temp = f_val * f_val - u
            s_next = d + temp
            u = (s_next - d) - temp
            d = s_next
            
            temp = a_val * a_val - M
            s_next = m + temp
            M = (s_next - m) - temp
            m = s_next
            
            temp = f_val * a_val - p
            s_next = g + temp
            p = (s_next - g) - temp
            g = s_next
            y += 1
    w = (g * g) / (d * m) if (d > 1e-30 and m > 1e-30 and y > 16) else 0.0
    return {"q": max(0.0, 1.0 - w), "g": g / m if m > 1e-30 else 0.0}

def v(e, t, l, n=0, a=1e99):
    r = isinstance(l, int) or (isinstance(l, float) and l.is_integer())
    if r:
        l = int(l)
        o = None
    else:
        o = E(l)
    i_len = len(e[0])
    f_len = len(t[0])
    h = len(e)
    s = int(max(n, 0, -l if r else 31 - o["base"]))
    c = int(min(a, i_len, f_len - l if r else f_len - o["base"] - 32))
    
    d, m = 0.0, 0.0
    g, u = 0.0, 0.0
    if c <= s:
        return {"q": 1.0, "g": 0.0}
    
    for n_idx in range(s, c):
        for a_idx in range(h):
            if r:
                i_val = t[a_idx][n_idx + l]
            else:
                i_val = 0.0
                e_base = n_idx + o["base"] - 31
                t_arr = t[a_idx]
                for t_idx in range(64):
                    i_val += t_arr[e_base + t_idx] * o["h"][t_idx]
            
            temp = e[a_idx][n_idx] * i_val - g
            h_val = d + temp
            g = (h_val - d) - temp
            d = h_val
            
            temp = i_val * i_val - u
            h_val = m + temp
            u = (h_val - m) - temp
            m = h_val
            
    M = d / m if m > 1e-30 else 0.0
    p, y_val = 0.0, 0.0
    w, x = 0.0, 0.0
    
    for n_idx in range(s, c):
        for a_idx in range(h):
            if r:
                i_val = t[a_idx][n_idx + l]
            else:
                i_val = 0.0
                e_base = n_idx + o["base"] - 31
                t_arr = t[a_idx]
                for t_idx in range(64):
                    i_val += t_arr[e_base + t_idx] * o["h"][t_idx]
            
            f_val = e[a_idx][n_idx]
            temp = (f_val - M * i_val) ** 2 - w
            s_val = p + temp
            w = (s_val - p) - temp
            p = s_val
            
            temp = f_val * f_val - x
            s_val = y_val + temp
            x = (s_val - y_val) - temp
            y_val = s_val
            
    return {"q": p / y_val if y_val > 1e-30 else 1.0, "g": M}

def golden_search(func, t, l, n=25):
    a = (math.sqrt(5.0) - 1.0) / 2.0
    r = l - a * (l - t)
    o = t + a * (l - t)
    i = func(r)
    f = func(o)
    for _ in range(n):
        if i < f:
            l = o
            o = r
            f = i
            r = l - a * (l - t)
            i = func(r)
        else:
            t = r
            r = o
            i = f
            o = t + a * (l - t)
            f = func(o)
    return r if i < f else o

def get_aligned_blocks(ref, comp, delay, lo=0, hi=1e99):
    r = isinstance(delay, int) or (isinstance(delay, float) and delay.is_integer())
    if r:
        delay = int(delay)
        o = None
    else:
        o = E(delay)
    i_len = len(ref[0])
    f_len = len(comp[0])
    h = len(ref)
    
    s = int(max(lo, 0, -delay if r else 31 - o["base"]))
    c = int(min(hi, i_len, f_len - delay if r else f_len - o["base"] - 32))
    d_len = max(0, c - s)
    
    A_block = [0.0] * (d_len * h)
    B_block = [0.0] * (d_len * h)
    
    for n in range(d_len):
        for a in range(h):
            if r:
                i_val = comp[a][s + n + delay]
            else:
                i_val = 0.0
                e_base = s + n + o["base"] - 31
                c_arr = comp[a]
                for t_idx in range(64):
                    i_val += c_arr[e_base + t_idx] * o["h"][t_idx]
            A_block[n * h + a] = ref[a][s + n]
            B_block[n * h + a] = i_val
            
    return A_block, B_block, d_len, s

def get_gain_offset(A_block, B_block):
    num = 0.0
    den = 0.0
    for i in range(len(A_block)):
        num += A_block[i] * B_block[i]
        den += B_block[i] * B_block[i]
    g = num / den if den > 1e-30 else 0.0
    return g

def b_lufs(e, sample_rate, num_channels=1):
    t_val = 0.7071752369554196
    l_val = math.tan(1681.974450955533 * math.pi / sample_rate)
    n_val = 1.5848647011308556
    a_val = 1.2587209302325617
    r_val = 1.0 + l_val / t_val + l_val * l_val
    
    b1 = [(n_val + a_val * l_val / t_val + l_val * l_val) / r_val,
          2.0 * (l_val * l_val - n_val) / r_val,
          (n_val - a_val * l_val / t_val + l_val * l_val) / r_val]
    a1 = [1.0,
          2.0 * (l_val * l_val - 1.0) / r_val,
          (1.0 - l_val / t_val + l_val * l_val) / r_val]
          
    t_val2 = 0.5003270373238773
    l_val2 = math.tan(38.13547087602444 * math.pi / sample_rate)
    n_val2 = 1.0 + l_val2 / t_val2 + l_val2 * l_val2
    
    b2 = [1.0, -2.0, 1.0]
    a2 = [1.0,
          2.0 * (l_val2 * l_val2 - 1.0) / n_val2,
          (1.0 - l_val2 / t_val2 + l_val2 * l_val2) / n_val2]
          
    class Biquad:
        def __init__(self, b, a):
            self.b = b
            self.a = a
            self.x1 = 0.0
            self.x2 = 0.0
            self.y1 = 0.0
            self.y2 = 0.0
        def process(self, x):
            y = self.b[0] * x + self.b[1] * self.x1 + self.b[2] * self.x2 - self.a[1] * self.y1 - self.a[2] * self.y2
            self.x2 = self.x1
            self.x1 = x
            self.y2 = self.y1
            self.y1 = y
            return y

    n_blocks = len(e) // num_channels
    if n_blocks == 0:
        return {"I": -float('inf'), "e": 0.0}
        
    filters = []
    for ch in range(num_channels):
        filters.append([Biquad(b1, a1), Biquad(b2, a2)])
        
    i_filtered = [0.0] * (n_blocks * num_channels)
    f_weights = [1.0] * num_channels
    if num_channels == 6:
        f_weights = [1.0, 1.0, 1.0, 0.0, 1.41, 1.41]
        
    for t in range(n_blocks):
        for n in range(num_channels):
            sample = e[t * num_channels + n]
            y1 = filters[n][0].process(sample)
            y2 = filters[n][1].process(y1)
            i_filtered[t * num_channels + n] = y2
            
    h = min(n_blocks, int(round(0.4 * sample_rate)))
    s = max(1, int(round(0.1 * sample_rate)))
    c = []
    d_sum = 0.0
    for idx in range(n_blocks):
        for ch in range(num_channels):
            val = i_filtered[idx * num_channels + ch]
            d_sum += f_weights[ch] * val * val
            
    for start_idx in range(0, n_blocks - h + 1, s):
        t_sum = 0.0
        for ch in range(num_channels):
            a_sum = 0.0
            for t_idx in range(h):
                val = i_filtered[(start_idx + t_idx) * num_channels + ch]
                a_sum += val * val
            t_sum += f_weights[ch] * a_sum / h
        c.append(t_sum)
        
    if not c:
        c = [d_sum / n_blocks]
        
    m = [val for val in c if 10.0 * math.log10(val + 1e-300) - 0.691 >= -70.0]
    if not m:
        return {"I": -float('inf'), "e": d_sum / n_blocks}
        
    g_avg = sum(m) / len(m)
    u_thresh = 10.0 * math.log10(g_avg + 1e-300) - 0.691 - 10.0
    M_vals = [val for val in m if 10.0 * math.log10(val + 1e-300) - 0.691 >= u_thresh]
    p_avg = sum(M_vals) / len(M_vals)
    
    return {"I": 10.0 * math.log10(p_avg + 1e-300) - 0.691, "e": d_sum / n_blocks}

def R_fft(e, t, inverse=False):
    n = len(e)
    a = 0
    for l in range(1, n):
        r = n >> 1
        while a & r:
            a ^= r
            r >>= 1
        a ^= r
        if l < a:
            e[l], e[a] = e[a], e[l]
            t[l], t[a] = t[a], t[l]
            
    a_size = 2
    while a_size <= n:
        angle = (2.0 if inverse else -2.0) * math.pi / a_size
        o = math.cos(angle)
        i = math.sin(angle)
        for l in range(0, n, a_size):
            n_w = 1.0
            r_w = 0.0
            half = a_size // 2
            for f in range(half):
                h = l + f
                s = h + half
                c = n_w * e[s] - r_w * t[s]
                d = n_w * t[s] + r_w * e[s]
                e[s] = e[h] - c
                t[s] = t[h] - d
                e[h] += c
                t[h] += d
                m = n_w
                n_w = m * o - r_w * i
                r_w = m * i + r_w * o
        a_size <<= 1
        
    if inverse:
        for l in range(n):
            e[l] /= n
            t[l] /= n

def compute_mrstft(ref, comp, channels=1):
    fft_sizes = [512, 1024, 2048]
    r = [0.0] * channels
    o = [0.0] * channels
    i_db = [0.0] * channels
    f_cnt = [0.0] * channels
    s_diff = 0.0
    c_sum = 0.0
    
    m = len(ref) // channels
    
    for g in range(channels):
        for u in range(len(fft_sizes)):
            M = fft_sizes[u]
            p = M // 2
            y = [0.0] * M
            for e in range(M):
                y[e] = 0.5 - 0.5 * math.cos(2.0 * math.pi * e / (M - 1))
                
            for start in range(0, m - M + 1, p):
                w_r = [0.0] * M
                w_i = [0.0] * M
                b_r = [0.0] * M
                b_i = [0.0] * M
                for idx in range(M):
                    w_r[idx] = ref[(start + idx) * channels + g] * y[idx]
                    b_r[idx] = comp[(start + idx) * channels + g] * y[idx]
                    
                R_fft(w_r, w_i)
                R_fft(b_r, b_i)
                
                for bin_idx in range(M // 2 + 1):
                    t_val = math.hypot(w_r[bin_idx], w_i[bin_idx])
                    l_val = math.hypot(b_r[bin_idx], b_i[bin_idx])
                    n_val = t_val - l_val
                    
                    r[g] += n_val * n_val
                    o[g] += t_val * t_val
                    s_diff += abs(n_val)
                    c_sum += t_val
                    
                    i_db[g] += abs(20.0 * math.log10((t_val + 1e-15) / (l_val + 1e-15)))
                    f_cnt[g] += 1
                    
    sc = [math.sqrt(r[g] / (o[g] if o[g] > 0 else 1.0)) for g in range(channels)]
    ls = [i_db[g] / (f_cnt[g] if f_cnt[g] > 0 else 1.0) for g in range(channels)]
    mr = s_diff / (c_sum if c_sum > 0 else 1.0)
    
    return {"sc": sc, "ls": ls, "mr": mr}

def o_db(e):
    return 20.0 * math.log10(e) if e > 0.0 else -float('inf')

def rms(e):
    t = 0.0
    l = 0.0
    for n in e:
        val = n * n - l
        a = t + val
        l = (a - t) - val
        t = a
    return math.sqrt(t / len(e)) if len(e) > 0 else 0.0

def peak(e):
    return max(abs(x) for x in e)

def run_null_test_on_files(ref_path, comp_path, expected_delay):
    # Load WAVs
    ref_samples, sample_rate, num_channels = load_wav_manual(ref_path)
    comp_samples, _, _ = load_wav_manual(comp_path)
    
    # 2D representation
    c_ref = [ref_samples]
    c_comp = [comp_samples]
    
    # Search window around expected delay
    min_delay = expected_delay - 3
    max_delay = expected_delay + 3
    
    r = int(math.floor(min_delay) - 1)
    o_val = int(math.floor(max_delay) + 1)
    lo = int(max(0, 31 - r))
    hi = int(min(len(c_ref[0]), len(c_comp[0]) - o_val - 32))
    
    # Coarse search
    best_int = int(round(expected_delay))
    best_q = 999.0
    for d in range(best_int - 3, best_int + 4):
        q_val = C(c_ref, c_comp, float(d), 1, lo, hi)["q"]
        if q_val < best_q:
            best_q = q_val
            best_int = d
            
    # Coarse fractional search
    best_coarse = best_int
    best_coarse_q = 999.0
    for step in range(-6, 7):
        d = best_int + step * 0.125
        q_val = C(c_ref, c_comp, d, 1, lo, hi)["q"]
        if q_val < best_coarse_q:
            best_coarse_q = q_val
            best_coarse = d
            
    # Fine search (golden section)
    best_delay = golden_search(lambda d: C(c_ref, c_comp, d, 1, lo, hi)["q"], best_coarse - 0.125, best_coarse + 0.125, 14)
    
    # Get aligned blocks
    A_block, B_block, n_aligned, start_idx = get_aligned_blocks(c_ref, c_comp, best_delay, lo, hi)
    
    # Gain offset
    gain = get_gain_offset(A_block, B_block)
    
    # Compute subtraction
    # V[e] = gain * B_block[e], $[e] = A_block[e] - V[e]
    subtraction = [0.0] * len(A_block)
    scaled_comp = [0.0] * len(B_block)
    for i in range(len(A_block)):
        scaled_comp[i] = gain * B_block[i]
        subtraction[i] = A_block[i] - scaled_comp[i]
        
    # LUFS calculation
    H = b_lufs(A_block, sample_rate, 1)
    G = b_lufs(B_block, sample_rate, 1)
    z = b_lufs(subtraction, sample_rate, 1)
    
    # RMS and Peaks
    J = rms(A_block)
    K = rms(B_block)
    Q = rms(subtraction)
    X = peak(A_block)
    Y = peak(subtraction)
    Z = o_db(Y) - o_db(X)
    
    # ESR & MSE
    ee = Q * Q * len(subtraction)
    te = J * J * len(A_block)
    le = ee / te if te > 0 else float('inf')
    ne = ee / len(subtraction)
    
    # MAE
    ae = sum(abs(x) for x in subtraction) / len(subtraction)
    
    # MRSTFT
    re = compute_mrstft(A_block, scaled_comp, 1)
    
    null_depth_lufs = z["I"] - H["I"]
    null_depth_rms = o_db(Q) - o_db(J)
    null_depth_peak = Z
    
    return {
        "delay": best_delay,
        "gain_db": o_db(abs(gain)),
        "null_lufs": null_depth_lufs,
        "null_rms": null_depth_rms,
        "null_peak": null_depth_peak,
        "esr": le,
        "mae": ae,
        "mse": ne,
        "mrstft": re["mr"]
    }

# Run a test and verify
exp_delay = (129 - 1) / 2.0 + 31.5 # 95.5
res = run_null_test_on_files('NeuralAmpModelerCore/example_audio/input.wav', 'test_out.wav', exp_delay)
print("TEST RUN RESULTS (taps=129, beta=10.5, cutoff=0.75):")
print(f"Delay offset: {res['delay']:.6f} samples")
print(f"Gain offset: {res['gain_db']:.4f} dB")
print(f"Null Depth LUFS: {res['null_lufs']:.2f} dB")
print(f"Null Depth RMS: {res['null_rms']:.2f} dB")
print(f"Null Depth Peak: {res['null_peak']:.2f} dB")
print(f"ESR: {res['esr']:.8f}")
print(f"MAE: {res['mae']:.8f}")
print(f"MSE: {res['mse']:.8f}")
print(f"MRSTFT: {res['mrstft']:.8f}")
