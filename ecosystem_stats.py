#!/usr/bin/env python3
"""
Marine Ecosystem Live Statistics Monitor
Single script launched by pressing 'S' in simulation
Reads data from simulation_stats.tmp written by C program
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
        
        # State
        self.stats_file = "simulation_stats.tmp"
        self.start_time = time.time()
        self.running = False
        self.update_thread = None
        self.simulation_connected = False
        
        self.setup_gui()
        self.auto_start()
    
    def setup_gui(self):
        """Setup compact GUI optimized for quick viewing"""
        self.root = tk.Tk()
        self.root.title("Ecosystem Stats")
        self.root.geometry("1000x600")
        
        # Main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Status bar at top
        status_frame = ttk.Frame(main_frame)
        status_frame.pack(fill=tk.X, pady=(0, 5))
        
        ttk.Label(status_frame, text="Marine Ecosystem Stats", 
                 font=("Arial", 14, "bold")).pack(side=tk.LEFT)
        
        self.status_label = ttk.Label(status_frame, text="Connecting...")
        self.status_label.pack(side=tk.RIGHT)
        
        # Three plots side by side
        plot_frame = ttk.Frame(main_frame)
        plot_frame.pack(fill=tk.BOTH, expand=True)
        
        self.canvas_width = 320
        self.canvas_height = 240
        
        # Nutrition Plot
        nutr_frame = ttk.LabelFrame(plot_frame, text="Nutrition Balance")
        nutr_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 3))
        
        self.nutrition_canvas = tk.Canvas(nutr_frame, width=self.canvas_width, height=self.canvas_height,
                                         bg='white', bd=1, relief=tk.SOLID)
        self.nutrition_canvas.pack(padx=5, pady=5)
        
        # Fish Plot
        fish_frame = ttk.LabelFrame(plot_frame, text="Fish Population")
        fish_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=3)
        
        self.fish_canvas = tk.Canvas(fish_frame, width=self.canvas_width, height=self.canvas_height,
                                    bg='white', bd=1, relief=tk.SOLID)
        self.fish_canvas.pack(padx=5, pady=5)
        
        # Plant Plot
        plant_frame = ttk.LabelFrame(plot_frame, text="Plant Nodes")
        plant_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(3, 0))
        
        self.plant_canvas = tk.Canvas(plant_frame, width=self.canvas_width, height=self.canvas_height,
                                     bg='white', bd=1, relief=tk.SOLID)
        self.plant_canvas.pack(padx=5, pady=5)
        
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
        ttk.Button(btn_frame, text="Close", command=self.close_window).pack(fill=tk.X, pady=1)
        
        # Initial message
        self.log("Ecosystem Statistics Monitor")
        self.log("Reading data from simulation_stats.tmp")
        
        self.root.protocol("WM_DELETE_WINDOW", self.close_window)
    
    def log(self, message):
        """Add timestamped message to log"""
        timestamp = time.strftime("%H:%M:%S")
        self.info_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.info_text.see(tk.END)
        self.root.update_idletasks()
    
    def read_stats_file(self):
        """Read current stats from C simulation"""
        try:
            if not os.path.exists(self.stats_file):
                return None, None, None
            
            # Check if file is recent (simulation running)
            file_age = time.time() - os.path.getmtime(self.stats_file)
            if file_age > 3.0:  # 3 second tolerance
                if self.simulation_connected:
                    self.log("Lost connection to simulation")
                    self.simulation_connected = False
                return None, None, None
            
            # Read binary data: nutrition_balance, fish_count, plant_count
            with open(self.stats_file, 'rb') as f:
                data = f.read(12)  # 3 floats = 12 bytes
                if len(data) == 12:
                    nutrition, fish_f, plant_f = struct.unpack('fff', data)
                    fish_count = int(fish_f)
                    plant_count = int(plant_f)
                    
                    if not self.simulation_connected:
                        self.log(f"Connected: {fish_count} fish, {plant_count} plants, balance {nutrition:.2f}")
                        self.simulation_connected = True
                    
                    return fish_count, plant_count, nutrition
                    
        except (IOError, struct.error) as e:
            if self.simulation_connected:
                self.log(f"Read error: {e}")
            return None, None, None
        
        return None, None, None
    
    def update_data(self):
        """Update data collections"""
        fish_count, plant_count, nutrition_balance = self.read_stats_file()
        
        if fish_count is None:
            # No data available - use last known or zero
            if self.fish_count:
                fish_count = self.fish_count[-1]
                plant_count = self.plant_count[-1] 
                nutrition_balance = self.nutrition_balance[-1]
            else:
                fish_count, plant_count, nutrition_balance = 0, 0, 0.0
        
        current_time = time.time() - self.start_time
        
        self.time_points.append(current_time)
        self.fish_count.append(fish_count)
        self.plant_count.append(plant_count)
        self.nutrition_balance.append(nutrition_balance)
        
        return fish_count, plant_count, nutrition_balance
    
    def draw_plot(self, canvas, data, title, color, min_val=None):
        """Draw a simple line plot"""
        canvas.delete("all")
        
        if len(data) < 2:
            canvas.create_text(self.canvas_width//2, self.canvas_height//2, 
                             text="Waiting for data...", font=("Arial", 10))
            return
        
        margin = 30
        plot_width = self.canvas_width - 2 * margin
        plot_height = self.canvas_height - 2 * margin
        
        # Data range
        if min_val is None:
            min_val = min(data)
        max_val = max(data)
        
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
        
        # Plot data
        points = []
        time_range = max(self.time_points) - min(self.time_points) if self.time_points else 1
        
        for t, val in zip(self.time_points, data):
            x = margin + ((t - min(self.time_points)) / time_range) * plot_width
            y = margin + plot_height - ((val - min_val) / data_range) * plot_height
            points.extend([x, y])
        
        # Draw line
        if len(points) >= 4:
            canvas.create_line(points, fill=color, width=2, smooth=True)
        
        # Current value dot
        if points:
            last_x, last_y = points[-2], points[-1]
            canvas.create_oval(last_x-2, last_y-2, last_x+2, last_y+2, fill=color, outline="red")
        
        # Title and current value
        canvas.create_text(self.canvas_width//2, 15, text=title, font=("Arial", 10, "bold"))
        
        if data:
            current = data[-1]
            canvas.create_text(self.canvas_width - 10, margin + 10, 
                             text=f"{current:.1f}", font=("Arial", 9, "bold"), 
                             fill=color, anchor=tk.E)
        
        # Y-axis labels
        for i in range(3):
            val = min_val + (i * data_range / 2)
            y = margin + plot_height - (i * plot_height / 2)
            canvas.create_text(margin - 5, y, text=f"{val:.0f}", font=("Arial", 8), anchor=tk.E)
    
    def update_plots(self):
        """Update all plots"""
        if not self.time_points:
            return
        
        self.draw_plot(self.nutrition_canvas, self.nutrition_balance, "Nutrition", "green")
        self.draw_plot(self.fish_canvas, self.fish_count, "Fish", "blue", min_val=0)
        self.draw_plot(self.plant_canvas, self.plant_count, "Plants", "brown", min_val=0)
    
    def monitoring_loop(self):
        """Main monitoring loop"""
        while self.running:
            try:
                fish_count, plant_count, nutrition_balance = self.update_data()
                
                # Update GUI
                self.root.after_idle(self.update_plots)
                
                # Update status
                elapsed = int(time.time() - self.start_time)
                mins, secs = divmod(elapsed, 60)
                status_text = f"{'LIVE' if self.simulation_connected else 'NO DATA'} | {mins:02d}:{secs:02d} | Fish:{fish_count} Plants:{plant_count} Nutrition:{nutrition_balance:.1f}"
                self.root.after_idle(lambda: self.status_label.config(text=status_text))
                
                # No automatic logging of data changes - keep terminal clean
                
                time.sleep(1.0)
                
            except Exception as e:
                self.root.after_idle(lambda: self.log(f"Error: {e}"))
                time.sleep(1.0)
    
    def auto_start(self):
        """Auto-start monitoring"""
        self.running = True
        self.update_thread = threading.Thread(target=self.monitoring_loop, daemon=True)
        self.update_thread.start()
        self.log("Monitoring started automatically")
    
    def clear_data(self):
        """Clear all data and restart"""
        self.time_points.clear()
        self.fish_count.clear()
        self.plant_count.clear()
        self.nutrition_balance.clear()
        self.start_time = time.time()
        self.log("Data cleared")
    
    def close_window(self):
        """Close window cleanly"""
        self.running = False
        self.log("Closing...")
        self.root.after(100, self.root.destroy)
    
    def run(self):
        """Start the GUI"""
        self.root.mainloop()

def main():
    """Main entry point"""
    print("Marine Ecosystem Statistics Monitor")
    print("Reading live data from simulation_stats.tmp")
    
    try:
        stats = EcosystemStats()
        stats.run()
    except KeyboardInterrupt:
        print("\nShutdown requested")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()