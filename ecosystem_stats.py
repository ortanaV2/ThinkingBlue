#!/usr/bin/env python3
"""
Marine Ecosystem Live Statistics Monitor with Temperature Control
Combined version: Beautiful plots + Temperature slider
"""

import tkinter as tk
from tkinter import ttk
import threading
import time
import math
import os
import struct
from collections import deque

class EcosystemStats:
    def __init__(self):
        self.max_data_points = 300  # 10 minutes at 30 FPS
        
        # Data storage
        self.time_points = deque(maxlen=self.max_data_points)
        self.nutrition_balance = deque(maxlen=self.max_data_points)
        self.fish_count = deque(maxlen=self.max_data_points)
        self.plant_count = deque(maxlen=self.max_data_points)
        self.temperature_data = deque(maxlen=self.max_data_points)
        self.bleached_count_data = deque(maxlen=self.max_data_points)
        
        # State
        self.stats_file = "simulation_stats.tmp"
        self.start_time = time.time()
        self.running = False
        self.update_thread = None
        self.simulation_connected = False
        self.current_temperature = 0.0
        self.bleached_count = 0
        
        self.setup_gui()
        self.auto_start()
    
    def setup_gui(self):
        """Setup beautiful GUI with plots and temperature control"""
        self.root = tk.Tk()
        self.root.title("Ecosystem Stats - Temperature Control")
        self.root.geometry("1200x750")
        
        # Main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Status bar at top
        status_frame = ttk.Frame(main_frame)
        status_frame.pack(fill=tk.X, pady=(0, 5))
        
        ttk.Label(status_frame, text="Marine Ecosystem Stats - Temperature Control", 
                 font=("Arial", 14, "bold")).pack(side=tk.LEFT)
        
        self.status_label = ttk.Label(status_frame, text="Connecting...")
        self.status_label.pack(side=tk.RIGHT)
        
        # Temperature control panel
        temp_frame = ttk.LabelFrame(main_frame, text="Temperature Control (Coral Bleaching)")
        temp_frame.pack(fill=tk.X, pady=(0, 5))
        
        temp_control_frame = ttk.Frame(temp_frame)
        temp_control_frame.pack(fill=tk.X, padx=10, pady=5)
        
        ttk.Label(temp_control_frame, text="Temperature:").pack(side=tk.LEFT)
        
        self.temp_var = tk.DoubleVar(value=0.0)
        self.temp_scale = ttk.Scale(temp_control_frame, from_=0.0, to=3.0, 
                                   variable=self.temp_var, orient=tk.HORIZONTAL,
                                   command=self.on_temperature_change)
        self.temp_scale.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(10, 10))
        
        self.temp_label = ttk.Label(temp_control_frame, text="0.0°C")
        self.temp_label.pack(side=tk.LEFT)
        
        self.bleached_label = ttk.Label(temp_control_frame, text="Bleached: 0")
        self.bleached_label.pack(side=tk.LEFT, padx=(20, 0))
        
        # Warning label
        self.warning_label = ttk.Label(temp_frame, text="Coral bleaching starts at temperatures > 0°C", 
                                      foreground="green", font=("Arial", 9))
        self.warning_label.pack(pady=2)
        
        # Four plots in 2x2 grid
        plot_frame = ttk.Frame(main_frame)
        plot_frame.pack(fill=tk.BOTH, expand=True)
        
        self.canvas_width = 290
        self.canvas_height = 200
        
        # Top row
        top_frame = ttk.Frame(plot_frame)
        top_frame.pack(fill=tk.BOTH, expand=True)
        
        # Nutrition Plot
        nutr_frame = ttk.LabelFrame(top_frame, text="Nutrition Balance")
        nutr_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 3))
        
        self.nutrition_canvas = tk.Canvas(nutr_frame, width=self.canvas_width, height=self.canvas_height,
                                         bg='white', bd=1, relief=tk.SOLID)
        self.nutrition_canvas.pack(padx=5, pady=5)
        
        # Fish Plot
        fish_frame = ttk.LabelFrame(top_frame, text="Fish Population")
        fish_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=3)
        
        self.fish_canvas = tk.Canvas(fish_frame, width=self.canvas_width, height=self.canvas_height,
                                    bg='white', bd=1, relief=tk.SOLID)
        self.fish_canvas.pack(padx=5, pady=5)
        
        # Bottom row
        bottom_frame = ttk.Frame(plot_frame)
        bottom_frame.pack(fill=tk.BOTH, expand=True)
        
        # Plant Plot
        plant_frame = ttk.LabelFrame(bottom_frame, text="Plant Nodes")
        plant_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 3))
        
        self.plant_canvas = tk.Canvas(plant_frame, width=self.canvas_width, height=self.canvas_height,
                                     bg='white', bd=1, relief=tk.SOLID)
        self.plant_canvas.pack(padx=5, pady=5)
        
        # Temperature & Bleaching Plot
        temp_plot_frame = ttk.LabelFrame(bottom_frame, text="Temperature & Bleached Corals")
        temp_plot_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=3)
        
        self.temp_canvas = tk.Canvas(temp_plot_frame, width=self.canvas_width, height=self.canvas_height,
                                    bg='white', bd=1, relief=tk.SOLID)
        self.temp_canvas.pack(padx=5, pady=5)
        
        # Bottom info panel
        info_frame = ttk.Frame(main_frame)
        info_frame.pack(fill=tk.X, pady=(5, 0))
        
        self.info_text = tk.Text(info_frame, height=5, wrap=tk.WORD, font=("Courier", 9))
        scrollbar = ttk.Scrollbar(info_frame, orient=tk.VERTICAL, command=self.info_text.yview)
        self.info_text.configure(yscrollcommand=scrollbar.set)
        
        self.info_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Control buttons
        btn_frame = ttk.Frame(info_frame)
        btn_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
        
        ttk.Button(btn_frame, text="Clear", command=self.clear_data).pack(fill=tk.X, pady=1)
        ttk.Button(btn_frame, text="Reset Temp", command=self.reset_temperature).pack(fill=tk.X, pady=1)
        ttk.Button(btn_frame, text="Close", command=self.close_window).pack(fill=tk.X, pady=1)
        
        # Initial message
        self.log("Ecosystem Statistics Monitor with Temperature Control")
        self.log("Reading data from simulation_stats.tmp")
        self.log("Use temperature slider to induce coral bleaching")
        
        self.root.protocol("WM_DELETE_WINDOW", self.close_window)
    
    def on_temperature_change(self, value):
        """Handle temperature slider change"""
        try:
            temp = float(value)
            self.current_temperature = temp
            self.temp_label.config(text=f"{temp:.1f}°C")
            
            # Write temperature to control file for C simulation
            try:
                with open("temperature_control.tmp", "w") as f:
                    f.write(f"{temp:.1f}\n")
            except Exception as e:
                self.log(f"Failed to write temperature: {e}")
            
            # Update warning
            if temp > 0.0:
                self.warning_label.config(text=f"Temperature {temp:.1f}°C - Coral bleaching active!", 
                                         foreground="red")
            else:
                self.warning_label.config(text="Temperature 0.0°C - No coral bleaching", 
                                         foreground="green")
        except Exception as e:
            self.log(f"Temperature change error: {e}")
    
    def reset_temperature(self):
        """Reset temperature to 0°C"""
        self.temp_var.set(0.0)
        self.on_temperature_change("0.0")
        self.log("Temperature reset to 0.0°C")
    
    def log(self, message):
        """Add timestamped message to log"""
        try:
            timestamp = time.strftime("%H:%M:%S")
            self.info_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.info_text.see(tk.END)
            self.root.update_idletasks()
        except:
            print(f"[{time.strftime('%H:%M:%S')}] {message}")
    
    def read_stats_file(self):
        """Read current stats from C simulation"""
        try:
            if not os.path.exists(self.stats_file):
                return None, None, None, None
            
            # Check if file is recent (simulation running)
            file_age = time.time() - os.path.getmtime(self.stats_file)
            if file_age > 5.0:  # 5 second tolerance
                if self.simulation_connected:
                    self.log("Lost connection to simulation")
                    self.simulation_connected = False
                return None, None, None, None
            
            # Read binary data
            with open(self.stats_file, 'rb') as f:
                data = f.read()
                
                if len(data) == 16:  # New format: nutrition, fish, plants, temperature
                    nutrition, fish_f, plant_f, temp = struct.unpack('ffff', data)
                    fish_count = int(fish_f)
                    plant_count = int(plant_f)
                    
                    if not self.simulation_connected:
                        self.log(f"Connected: {fish_count} fish, {plant_count} plants, temp {temp:.1f}°C")
                        self.simulation_connected = True
                    
                    return fish_count, plant_count, nutrition, temp
                    
                elif len(data) == 12:  # Old format: nutrition, fish, plants
                    nutrition, fish_f, plant_f = struct.unpack('fff', data)
                    fish_count = int(fish_f)
                    plant_count = int(plant_f)
                    temp = 0.0  # Default temperature
                    
                    if not self.simulation_connected:
                        self.log(f"Connected (old format): {fish_count} fish, {plant_count} plants")
                        self.simulation_connected = True
                    
                    return fish_count, plant_count, nutrition, temp
                    
        except (IOError, struct.error) as e:
            if self.simulation_connected:
                self.log(f"Read error: {e}")
                self.simulation_connected = False
            return None, None, None, None
        
        return None, None, None, None
    
    def update_data(self):
        """Update data collections"""
        fish_count, plant_count, nutrition_balance, temperature = self.read_stats_file()
        
        if fish_count is None:
            # No data available - use last known or zero
            if self.fish_count:
                fish_count = self.fish_count[-1]
                plant_count = self.plant_count[-1] 
                nutrition_balance = self.nutrition_balance[-1]
                temperature = self.temperature_data[-1] if self.temperature_data else 0.0
            else:
                fish_count, plant_count, nutrition_balance, temperature = 0, 0, 0.0, 0.0
        
        current_time = time.time() - self.start_time
        
        self.time_points.append(current_time)
        self.fish_count.append(fish_count)
        self.plant_count.append(plant_count)
        self.nutrition_balance.append(nutrition_balance)
        self.temperature_data.append(temperature)
        
        # Estimate bleached count (will be updated from simulation later)
        estimated_bleached = int(plant_count * max(0, temperature - 0.5) * 0.2) if temperature > 0 else 0
        self.bleached_count_data.append(estimated_bleached)
        self.bleached_count = estimated_bleached
        
        return fish_count, plant_count, nutrition_balance, temperature
    
    def draw_plot(self, canvas, data, title, color, min_val=None, secondary_data=None, secondary_color=None):
        """Draw a beautiful line plot with optional secondary data"""
        canvas.delete("all")
        
        if len(data) < 2:
            canvas.create_text(self.canvas_width//2, self.canvas_height//2, 
                             text="Waiting for data...", font=("Arial", 10))
            return
        
        margin = 30
        plot_width = self.canvas_width - 2 * margin
        plot_height = self.canvas_height - 2 * margin
        
        # Data range for primary data
        if min_val is None:
            min_val = min(data)
        max_val = max(data)
        
        # Include secondary data in range if present
        if secondary_data and len(secondary_data) > 0:
            sec_min = min(secondary_data)
            sec_max = max(secondary_data)
            min_val = min(min_val, sec_min)
            max_val = max(max_val, sec_max)
        
        data_range = max_val - min_val
        if data_range == 0:
            data_range = 1
        
        # Add padding
        padding = data_range * 0.1
        min_val -= padding
        max_val += padding
        data_range = max_val - min_val
        
        # Draw axes
        canvas.create_line(margin, margin, margin, margin + plot_height, fill="black", width=1)
        canvas.create_line(margin, margin + plot_height, margin + plot_width, margin + plot_height, fill="black", width=1)
        
        # Draw grid
        for i in range(1, 5):
            y = margin + (i * plot_height / 5)
            canvas.create_line(margin, y, margin + plot_width, y, fill="lightgray", dash=(2, 2))
        
        # Draw temperature threshold line at 0°C if this is temperature plot
        if title == "Temperature & Bleached Corals" and data_range > 0:
            temp_zero_y = margin + plot_height - ((0.0 - min_val) / data_range) * plot_height
            if temp_zero_y >= margin and temp_zero_y <= margin + plot_height:
                canvas.create_line(margin, temp_zero_y, margin + plot_width, temp_zero_y, 
                                  fill="orange", width=2, dash=(5, 5))
                canvas.create_text(margin + plot_width - 30, temp_zero_y - 10, 
                                  text="0°C", fill="orange", font=("Arial", 8, "bold"))
        
        # Plot primary data
        if len(self.time_points) > 0:
            points = []
            time_range = max(self.time_points) - min(self.time_points) if len(self.time_points) > 1 else 1
            
            for t, val in zip(self.time_points, data):
                x = margin + ((t - min(self.time_points)) / time_range) * plot_width
                y = margin + plot_height - ((val - min_val) / data_range) * plot_height
                points.extend([x, y])
            
            # Draw primary line
            if len(points) >= 4:
                canvas.create_line(points, fill=color, width=2, smooth=True)
            
            # Draw secondary data if present
            if secondary_data and len(secondary_data) > 0:
                sec_points = []
                for t, val in zip(self.time_points, secondary_data):
                    x = margin + ((t - min(self.time_points)) / time_range) * plot_width
                    y = margin + plot_height - ((val - min_val) / data_range) * plot_height
                    sec_points.extend([x, y])
                
                if len(sec_points) >= 4:
                    canvas.create_line(sec_points, fill=secondary_color or "red", width=2, smooth=True, dash=(3, 3))
            
            # Current value dots
            if points:
                last_x, last_y = points[-2], points[-1]
                canvas.create_oval(last_x-2, last_y-2, last_x+2, last_y+2, fill=color, outline="darkred")
            
            if secondary_data and len(secondary_data) > 0 and len(sec_points) >= 2:
                last_x, last_y = sec_points[-2], sec_points[-1]
                canvas.create_oval(last_x-2, last_y-2, last_x+2, last_y+2, fill=secondary_color or "red", outline="darkred")
        
        # Title and current values
        canvas.create_text(self.canvas_width//2, 15, text=title, font=("Arial", 10, "bold"))
        
        if data:
            current = data[-1]
            if title == "Temperature & Bleached Corals":
                text = f"T:{current:.1f}°C"
                if secondary_data and len(secondary_data) > 0:
                    bleached = secondary_data[-1]
                    text += f" B:{bleached:.0f}"
            else:
                text = f"{current:.1f}"
            canvas.create_text(self.canvas_width - 10, margin + 10, 
                             text=text, font=("Arial", 9, "bold"), 
                             fill=color, anchor=tk.E)
        
        # Y-axis labels
        for i in range(3):
            val = min_val + (i * data_range / 2)
            y = margin + plot_height - (i * plot_height / 2)
            if title == "Temperature & Bleached Corals":
                label_text = f"{val:.1f}"
            else:
                label_text = f"{val:.0f}"
            canvas.create_text(margin - 5, y, text=label_text, font=("Arial", 8), anchor=tk.E)
    
    def update_plots(self):
        """Update all plots"""
        if not self.time_points:
            return
        
        try:
            self.draw_plot(self.nutrition_canvas, self.nutrition_balance, "Nutrition Balance", "green")
            self.draw_plot(self.fish_canvas, self.fish_count, "Fish Population", "blue", min_val=0)
            self.draw_plot(self.plant_canvas, self.plant_count, "Plant Nodes", "brown", min_val=0)
            self.draw_plot(self.temp_canvas, self.temperature_data, "Temperature & Bleached Corals", "red", 
                          secondary_data=self.bleached_count_data, secondary_color="gray")
        except Exception as e:
            self.log(f"Plot update error: {e}")
    
    def monitoring_loop(self):
        """Main monitoring loop"""
        while self.running:
            try:
                fish_count, plant_count, nutrition_balance, temperature = self.update_data()
                
                # Update GUI safely
                try:
                    self.root.after_idle(self.update_plots)
                except:
                    pass
                
                # Update bleached count display
                if self.simulation_connected:
                    try:
                        self.root.after_idle(lambda: self.bleached_label.config(text=f"Bleached: {self.bleached_count}"))
                    except:
                        pass
                
                # Update status
                elapsed = int(time.time() - self.start_time)
                mins, secs = divmod(elapsed, 60)
                status_text = f"{'LIVE' if self.simulation_connected else 'NO DATA'} | {mins:02d}:{secs:02d} | Fish:{fish_count} Plants:{plant_count} Temp:{temperature:.1f}°C"
                try:
                    self.root.after_idle(lambda: self.status_label.config(text=status_text))
                except:
                    pass
                
                time.sleep(1.0)
                
            except Exception as e:
                try:
                    self.root.after_idle(lambda: self.log(f"Monitor error: {e}"))
                except:
                    print(f"Monitor error: {e}")
                time.sleep(2.0)
    
    def auto_start(self):
        """Auto-start monitoring"""
        self.running = True
        self.update_thread = threading.Thread(target=self.monitoring_loop, daemon=True)
        self.update_thread.start()
        self.log("Monitoring started with temperature control")
    
    def clear_data(self):
        """Clear all data and restart"""
        self.time_points.clear()
        self.fish_count.clear()
        self.plant_count.clear()
        self.nutrition_balance.clear()
        self.temperature_data.clear()
        self.bleached_count_data.clear()
        self.start_time = time.time()
        self.log("Data cleared")
    
    def close_window(self):
        """Close window cleanly"""
        self.running = False
        
        # Clean up temperature control file
        try:
            os.remove("temperature_control.tmp")
        except:
            pass
        
        self.log("Closing...")
        self.root.after(100, self.root.destroy)
    
    def run(self):
        """Start the GUI"""
        self.root.mainloop()

def main():
    """Main entry point"""
    print("Marine Ecosystem Statistics Monitor with Temperature Control")
    print("Reading live data from simulation_stats.tmp")
    print("Beautiful plots + Temperature slider for coral bleaching")
    
    try:
        stats = EcosystemStats()
        stats.run()
    except KeyboardInterrupt:
        print("\nShutdown requested")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()