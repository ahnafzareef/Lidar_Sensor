% Deliverable 2

clear; clc; close all;

%% Configuration
port = "COM7";          
baudrate = 115200;
NUM_SCANS = 5;
NUM_ANGLES = 128;
SCAN_SPACING_MM = 300;  % 30 cm apart in code

%% Open serial port
device = serialport(port, baudrate);
device.Timeout = 600;
configureTerminator(device, "CR/LF");

fprintf("Opening: %s\n", port);
flush(device);

input("Press Enter to start communication, then press PJ0 on the board...");

% If your MCU actually waits for 's', uncomment this:
% write(device, 's', "char");

%% Read UART data
data = [];  
% columns:
% [scanNum, angleDeg, X, Y, Z, distance]

while true
    line = readline(device);
    line = strtrim(string(line));
    fprintf("%s\n", line);

   if line == "END"
    break;
   end
    
    if line == "STOPPED"
        fprintf("Scan stopped early. Waiting for next press...\n");
        continue;
    end
    vals = sscanf(line, '%f,%f,%f');

    if numel(vals) ~= 3
        fprintf("Skipped invalid line: %s\n", line);
        continue;
    end

    scanNum = vals(1);
    angleDeg = vals(2);
    distance = vals(3);

    if distance <= 0 || distance > 4000 || distance == 65535
        fprintf("Rejected invalid distance at scan %d angle %g\n", scanNum, angleDeg);
        continue;
    end

    X = scanNum * SCAN_SPACING_MM;
    theta = deg2rad(angleDeg);

    % vertical scan plane = Y-Z
    Y = distance * cos(theta);
    Z = distance * sin(theta);

    data(end+1, :) = [scanNum, angleDeg, X, Y, Z, distance];
end

fprintf("Closing: %s\n", port);
clear device;

if isempty(data)
    error("No valid points received.");
end

xyz = data(:, 3:5);
writematrix(xyz, "deliverable2_xyz.txt", "Delimiter", "space");

%% Plot
figure;
hold on;
grid on;
axis equal;
view(3);

% Scatter all points
scatter3(data(:,3), data(:,4), data(:,5), 60, 'filled');

% Sort by scan, then angle
data = sortrows(data, [1 2]);

% 1) Connect each scan loop
for scanNum = 0:(NUM_SCANS-1)
    scanPts = data(data(:,1) == scanNum, :);

    if isempty(scanPts)
        continue;
    end

    % close the loop
    scanPts = [scanPts; scanPts(1,:)];

    plot3(scanPts(:,3), scanPts(:,4), scanPts(:,5), '-o', 'LineWidth', 2);
end

% 2) Connect same-angle points across scans
angles = unique(data(:,2));

for k = 1:length(angles)
    a = angles(k);
    colPts = data(data(:,2) == a, :);

    if size(colPts,1) < 2
        continue;
    end

    colPts = sortrows(colPts, 1);
    plot3(colPts(:,3), colPts(:,4), colPts(:,5), '-', 'LineWidth', 1.5);
end

xlabel('X (mm)');
ylabel('Y (mm)');
zlabel('Z (mm)');
title(' 3D ToF Scan');
hold off;
