import React, { useState } from "react";
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from "recharts";
import _ from "lodash";

const LogDashboard = () => {
  const [logData, setLogData] = useState([]);
  const [masterDeviceStats, setMasterDeviceStats] = useState({});
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState(null);
  const parseTimestamp = (timestampStr) => {
    // Example format: "2023-12-08 05:12:26,530"
    try {
      const [datePart, timePart] = timestampStr.split(' ');
      const [time, ms] = timePart.split(',');
      const fullTimestamp = `${datePart}T${time}.${ms}Z`;
      return new Date(fullTimestamp);
    } catch (err) {
      console.warn('Error parsing timestamp:', timestampStr, err);
      return new Date();
    }
  };
  const handleFileUpload = async (event) => {
    const files = event.target.files;
    if (files.length === 0) return;

    setIsProcessing(true);
    setError(null);

    try {
      const allData = [];

      for (const file of files) {
        const reader = new FileReader();
        const fileContent = await new Promise((resolve, reject) => {
          reader.onload = (e) => resolve(e.target.result);
          reader.onerror = (e) => reject(e);
          reader.readAsText(file);
        });

        const lines = fileContent.split("\n");

        for (let i = 0; i < lines.length; i++) {
          const line = lines[i].trim();
          if (line.includes("Raw Data:")) {
            try {
              const timestampLine = lines[i - 1];
              const timestamp = timestampLine.split(" - ")[0];
              const masterDeviceLine = timestampLine.split(" - ")[1];
              const masterDevice = masterDeviceLine
                .split(" | ")[0]
                .split(": ")[1];
              const duration = parseInt(
                masterDeviceLine.split(" | ")[1].split(": ")[1]
              );

              // Parse the raw data
              const rawDataStr = line.split("Raw Data: [")[1].split("]")[0];
              const devices = rawDataStr.split(",PR,").map((d) => {
                const parts = d.trim().replace(/[' ]/g, "").split(",");
                return {
                  id: parseInt(parts[0]),
                  status: parseInt(parts[1]),
                  type: parseInt(parts[2]),
                  value: parseInt(parts[3]),
                };
              });

              allData.push({
                timestamp: parseTimestamp(timestamp),
                masterDevice: parseInt(masterDevice),
                duration,
                devices,
              });
            } catch (err) {
              console.warn("Error parsing line:", line, err);
              continue;
            }
          }
        }
      }

      // Sort by timestamp
      allData.sort((a, b) => a.timestamp - b.timestamp);

      // Calculate master device statistics
      const deviceStats = _.groupBy(allData, "masterDevice");
      const stats = {};
      Object.keys(deviceStats).forEach((device) => {
        stats[device] = {
          count: deviceStats[device].length,
          avgDuration: _.meanBy(deviceStats[device], "duration"),
        };
      });

      setMasterDeviceStats(stats);
      setLogData(allData);
    } catch (error) {
      console.error("Error processing files:", error);
      setError(
        "Error processing log files. Please ensure the files are in the correct format."
      );
    } finally {
      setIsProcessing(false);
    }
  };
  
  const formatTimestamp = (timestamp) => {
    return new Date(timestamp).toLocaleTimeString();
  };

  const getChartData = () => {
    return logData.map((entry) => ({
      timestamp: formatTimestamp(entry.timestamp),
      device0: entry.devices[0].value,
      device1: entry.devices[1].value,
      device2: entry.devices[2].value,
      device3: entry.devices[3].value,
      device4: entry.devices[4].value,
      device5: entry.devices[5].value,
      masterDevice: entry.masterDevice,
      duration: entry.duration,
    }));
  };

  const containerStyle = {
    padding: "32px",
    backgroundColor: "#f8fafc",
    minHeight: "100vh",
    fontFamily: "system-ui, -apple-system, sans-serif",
  };

  const cardStyle = {
    backgroundColor: "white",
    borderRadius: "12px",
    boxShadow:
      "0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06)",
    padding: "24px",
    border: "1px solid #e2e8f0",
  };

  const uploadContainerStyle = {
    ...cardStyle,
    marginBottom: "32px",
    backgroundColor: "white",
    transition: "all 0.3s ease",
  };

  const fileInputStyle = {
    width: "100%",
    padding: "12px",
    border: "2px dashed #e2e8f0",
    borderRadius: "8px",
    cursor: "pointer",
    transition: "all 0.3s ease",
    backgroundColor: "#f8fafc",
    color: "#475569",
    fontSize: "0.875rem",
  };

  const statsGridStyle = {
    display: "grid",
    gridTemplateColumns: "repeat(auto-fit, minmax(280px, 1fr))",
    gap: "24px",
    marginBottom: "32px",
  };

  const statsCardStyle = {
    ...cardStyle,
    display: "flex",
    flexDirection: "column",
    gap: "12px",
  };

  const chartContainerStyle = {
    ...cardStyle,
    marginBottom: "32px",
    height: "480px",
  };

  const headingStyle = {
    fontSize: "1.25rem",
    fontWeight: "600",
    color: "#1e293b",
    marginBottom: "16px",
  };

  const statValueStyle = {
    fontSize: "2rem",
    fontWeight: "600",
    color: "#0f172a",
    marginBottom: "4px",
  };

  const statLabelStyle = {
    fontSize: "0.875rem",
    color: "#64748b",
  };

  const chartColors = {
    device0: "#3b82f6", // Blue
    device1: "#10b981", // Green
    device2: "#f59e0b", // Yellow
    device3: "#ef4444", // Red
    device4: "#8b5cf6", // Purple
    device5: "#ec4899", // Pink
    duration: "#3b82f6", // Blue
  };

  return (
    <div style={containerStyle}>
      <div style={{ maxWidth: "1400px", margin: "0 auto" }}>
        {/* Upload Section */}
        <div style={uploadContainerStyle}>
          <h2 style={headingStyle}>Upload Log Files</h2>
          <input
            type="file"
            multiple
            accept=".log"
            onChange={handleFileUpload}
            style={fileInputStyle}
          />
          {isProcessing && (
            <div
              style={{
                color: "#3b82f6",
                marginTop: "12px",
                fontSize: "0.875rem",
              }}
            >
              Processing files...
            </div>
          )}
          {error && (
            <div
              style={{
                color: "#ef4444",
                marginTop: "12px",
                fontSize: "0.875rem",
              }}
            >
              {error}
            </div>
          )}
        </div>

        {/* Stats Grid */}
        <div style={statsGridStyle}>
          {Object.entries(masterDeviceStats).map(([device, stats]) => (
            <div key={device} style={statsCardStyle}>
              <h3 style={headingStyle}>Master Device {device}</h3>
              <div>
                <div style={statValueStyle}>{stats.count}</div>
                <div style={statLabelStyle}>Total Events</div>
              </div>
              <div>
                <div style={statValueStyle}>
                  {stats.avgDuration.toFixed(2)}s
                </div>
                <div style={statLabelStyle}>Average Duration</div>
              </div>
            </div>
          ))}
        </div>

        {logData.length > 0 && (
          <>
            {/* Device Values Chart */}
            <div style={chartContainerStyle}>
              <h2 style={headingStyle}>Device Values Over Time</h2>
              <div style={{ height: "400px", width: "100%" }}>
                <ResponsiveContainer>
                  <LineChart
                    data={getChartData()}
                    margin={{ top: 10, right: 30, left: 10, bottom: 40 }}
                  >
                    <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
                    <XAxis
                      dataKey="timestamp"
                      tick={{ fill: "#64748b", fontSize: 12 }}
                      angle={-45}
                      textAnchor="end"
                      height={70}
                      stroke="#cbd5e1"
                    />
                    <YAxis
                      tick={{ fill: "#64748b", fontSize: 12 }}
                      stroke="#cbd5e1"
                    />
                    <Tooltip
                      contentStyle={{
                        backgroundColor: "white",
                        border: "1px solid #e2e8f0",
                        borderRadius: "6px",
                        boxShadow: "0 4px 6px -1px rgba(0, 0, 0, 0.1)",
                      }}
                    />
                    <Legend
                      wrapperStyle={{
                        paddingTop: "20px",
                      }}
                    />
                    {Object.keys(chartColors)
                      .slice(0, 6)
                      .map((device, index) => (
                        <Line
                          key={device}
                          type="monotone"
                          dataKey={device}
                          stroke={chartColors[device]}
                          name={`Device ${index}`}
                          dot={false}
                          strokeWidth={2}
                        />
                      ))}
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>

            {/* Duration Chart */}
            <div style={chartContainerStyle}>
              <h2 style={headingStyle}>Duration Analysis</h2>
              <div style={{ height: "400px", width: "100%" }}>
                <ResponsiveContainer>
                  <LineChart
                    data={getChartData()}
                    margin={{ top: 10, right: 30, left: 10, bottom: 40 }}
                  >
                    <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
                    <XAxis
                      dataKey="timestamp"
                      tick={{ fill: "#64748b", fontSize: 12 }}
                      angle={-45}
                      textAnchor="end"
                      height={70}
                      stroke="#cbd5e1"
                    />
                    <YAxis
                      tick={{ fill: "#64748b", fontSize: 12 }}
                      stroke="#cbd5e1"
                    />
                    <Tooltip
                      contentStyle={{
                        backgroundColor: "white",
                        border: "1px solid #e2e8f0",
                        borderRadius: "6px",
                        boxShadow: "0 4px 6px -1px rgba(0, 0, 0, 0.1)",
                      }}
                    />
                    <Legend
                      wrapperStyle={{
                        paddingTop: "20px",
                      }}
                    />
                    <Line
                      type="monotone"
                      dataKey="duration"
                      stroke={chartColors.duration}
                      name="Duration"
                      dot={false}
                      strokeWidth={2}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
          </>
        )}
      </div>
    </div>
  );
};

export default LogDashboard;
