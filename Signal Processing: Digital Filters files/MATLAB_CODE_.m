clc; clear; close all;
function f_est = quadratic_fft_peak_interp(mag, f_axis)
% mag - FFT magnitude (single-sided)
% f_axis - frequency axis corresponding to mag
%
% Returns:
% f_est - interpolated peak frequency estimate

[~, idx] = max(mag);

if idx == 1 || idx == length(mag)
    % Peak at spectrum edge; no interpolation possible
    f_est = f_axis(idx);
    return;
end

alpha = mag(idx-1);
beta = mag(idx);
gamma = mag(idx+1);

p = 0.5 * (alpha - gamma) / (alpha - 2*beta + gamma);

% Interpolated index location
idx_interp = idx + p;

% Interpolated frequency
delta_f = f_axis(2) - f_axis(1);
f_est = f_axis(1) + (idx_interp-1) * delta_f;

end

% SECTION 1 – Input and frequency extraction
id = input('Enter student number: ', 's');
id_digits = id(isstrprop(id, 'digit'));

if numel(id_digits) < 3
    error('Student number must have at least three numeric digits.');
end

non_zero = id_digits(id_digits ~= '0');
if numel(non_zero) >= 3
    digits3 = non_zero(end-2:end);
else
    digits3 = [repmat('0',1,3-numel(non_zero)) non_zero];
    digits3 = digits3(end-2:end);
end

dA = str2double(digits3(1));
dB = str2double(digits3(2));
dC = str2double(digits3(3));

f_A = dA * 1;
f_B = dB * 10;
f_C = dC * 100;

fprintf('\nDerived frequencies for Student %s:\n', id);
fprintf('  fA = %d Hz\n  fB = %d Hz\n  fC = %d Hz\n', f_A, f_B, f_C);

freq_list = [f_A, f_B, f_C];

% SECTION 2 – Signal generation and processing
Fs = 10000; % sampling rate
Ts = 1/Fs;
duration = 1; % seconds
t = 0:Ts:duration;
Vpk = 5;

for n = 1:length(freq_list)
    f0 = freq_list(n);
    fprintf('\n--- Processing %.0f Hz ---\n', f0);

    wave_square = Vpk * square(2*pi*f0*t);
    wave_sine   = Vpk * sin(2*pi*f0*t);

    % Filter design
    cutoff = 1.4 * f0;
    orderN = 6;
    [bLP, aLP] = butter(orderN, cutoff/(Fs/2));
    wave_out = filtfilt(bLP, aLP, wave_square);

    fprintf('  Designed 6th-order LPF with cutoff %.1f Hz\n', cutoff);

    % Frequency analysis
    N = numel(t);
    f_axis = Fs*(0:floor(N/2))/N;
    FFT_square = fft(wave_square)/N;
    FFT_sine   = fft(wave_out)/N;

    mag_sq  = abs(FFT_square(1:floor(N/2)+1));
    mag_out = abs(FFT_sine(1:floor(N/2)+1));
    mag_sq(2:end-1)  = 2*mag_sq(2:end-1);
    mag_out(2:end-1) = 2*mag_out(2:end-1);

    % Add noise and restore
    SNR_in = 10; % dB
    noisy_wave = awgn(wave_out, SNR_in, 'measured');
    cleaned_wave = filtfilt(bLP, aLP, noisy_wave);

    % Fix indexing warning: ensure samples_to_show is integer >= 1
    cycles_display = max(1, min(5, floor(duration * f0)));
    samples_to_show = min(length(t), round(Fs/f0 * cycles_display));
    samples_to_show = max(1, floor(samples_to_show));
    idx = 1:samples_to_show;

    figure('Name', sprintf('Signal %d Hz', f0), 'Position', [80 80 1200 800]);

    subplot(3,2,1);
    plot(t(idx), wave_square(idx)); grid on;
    title(sprintf('Square Input (%.0f Hz)', f0));
    xlabel('Time (s)'); ylabel('Amplitude (V)');

    subplot(3,2,2);
    plot(t(idx), wave_out(idx), 'r'); grid on;
    title('Filtered Output'); xlabel('Time (s)');

    subplot(3,2,3);
    stem(f_axis, mag_sq, 'b', 'Marker', 'none'); grid on;
    title('Spectrum – Square'); xlabel('Frequency (Hz)'); ylabel('|A|');

    subplot(3,2,4);
    stem(f_axis, mag_out, 'r', 'Marker', 'none'); grid on;
    title('Spectrum – Filtered Output'); xlabel('Frequency (Hz)');

    subplot(3,2,5);
    plot(t(idx), wave_out(idx), 'b', t(idx), noisy_wave(idx), 'm'); grid on;
    legend('Clean','Noisy'); title('Channel Transmission');

    subplot(3,2,6);
    plot(t(idx), noisy_wave(idx), 'm', t(idx), cleaned_wave(idx), 'g'); grid on;
    legend('Noisy','Recovered'); title('Noise Reduction Result');

    sgtitle(sprintf('Signal Analysis @ %.0f Hz', f0));

    % Performance metrics
    P_clean = mean(wave_out.^2);
    P_noise = mean((noisy_wave - wave_out).^2);
    est_SNR = 10*log10(P_clean / P_noise);

    P_after = mean((cleaned_wave - wave_out).^2);
    SNR_gain = 10*log10(P_noise / P_after);

    [~, idx_peak] = max(mag_out(1:round(2*f0/Fs*N)));
    freq_meas = f_axis(idx_peak);

    fprintf('  Input SNR: %.1f dB | Measured: %.2f dB\n', SNR_in, est_SNR);
    fprintf('  Post-filter improvement: %.2f dB\n', SNR_gain);
    fprintf('  Frequency check: %.2f Hz (Error = %.2f Hz)\n', freq_meas, abs(f0 - freq_meas));
end

fprintf('\nAnalysis complete – All GA2 outcomes demonstrated.\n');
