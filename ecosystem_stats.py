#!/usr/bin/env python3
"""
Marine Ecosystem Statistics Monitor
Real-time visualization with temperature control for ecosystem simulation
"""

import tkinter as tk
from tkinter import ttk
import threading
import time
import os
import struct
from collections import deque

class EcosystemStats:
    def __init__(self):
        print("Initializing Marine Ecosystem Statistics Monitor...")
        
        # Data storage configuration
        self.max_data_points = 300  # 10 minutes at 30 FPS
        
        # Initialize data containers
        self.time_points = deque(maxlen=self.max_data_points)
        self.total_environmental_nutrition = deque(maxlen=self.max_data_points)
        self.fish_count = deque(maxlen=self.max_data_points)
        self.plant_count = deque(maxlen=self.max_data_points)
        self.temperature_data = deque(maxlen=self.max_data_points)
        self.bleached_count_data = deque(maxlen=self.max_data_points)
        
        # System state
        self.stats_file = "simulation_stats.tmp"
        self.start_time = time.time()
        self.running = False
        self.update_thread = None
        self.simulation_connected = False
        self.current_temperature = 0.0
        self.bleached_count = 0
        
        print("Setting up GUI components...")
        self.setup_gui()
        print("Starting data monitoring...")
        self.auto_start()
    
    def setup_gui(self):
        """Initialize GUI components"""
        self.root = tk.Tk()
        self.root.title("Marine Ecosystem Statistics")
        self.root.geometry("1200x750")
        self.root.minsize(1080, 800)
        
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Header
        status_frame = ttk.Frame(main_frame)
        status_frame.pack(fill=tk.X, pady=(0, 5))
        
        ttk.Label(status_frame, text="Marine Ecosystem Statistics", 
                 font=("Arial", 14, "bold")).pack(side=tk.LEFT)
        
        self.connection_label = ttk.Label(status_frame, text="Connecting...", 
                                         foreground="orange")
        self.connection_label.pack(side=tk.RIGHT)
        
        # Temperature control panel
        self.setup_temperature_control(main_frame)
        
        # Charts section
        self.setup_charts(main_frame)
        
        # Info log
        self.setup_info_panel(main_frame)
        
        # Canvas dimensions (updated dynamically)
        self.canvas_width = 520
        self.canvas_height = 200
        
        self.root.bind('<Configure>', self.on_window_resize)
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
    
    def setup_temperature_control(self, parent):
        """Setup temperature control interface"""
        temp_frame = ttk.LabelFrame(parent, text="Temperature Control System", padding=15)
        temp_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Main controls row
        control_row1 = ttk.Frame(temp_frame)
        control_row1.pack(fill=tk.X, pady=(0, 10))
        
        # Temperature display
        temp_display_frame = ttk.Frame(control_row1)
        temp_display_frame.pack(side=tk.LEFT, padx=(0, 20))
        
        ttk.Label(temp_display_frame, text="Ocean Temperature Offset:", font=("Arial", 10, "bold")).pack()
        self.temp_label = ttk.Label(temp_display_frame, text="0.0°C", 
                                   font=("Arial", 16, "bold"), foreground="green")
        self.temp_label.pack()
        self.temp_label.configure(foreground="green")  # Ensure initial green color
        
        # Slider control
        slider_frame = ttk.Frame(control_row1)
        slider_frame.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 20))
        
        ttk.Label(slider_frame, text="Adjust Temperature Offset Range: 0°C - 3°C").pack()
        self.temp_var = tk.DoubleVar(value=0.0)
        temp_slider = tk.Scale(slider_frame, from_=0.0, to=3.0, resolution=0.1,
                              orient=tk.HORIZONTAL, variable=self.temp_var,
                              command=self.on_temperature_change, length=300,
                              font=("Arial", 9), bg="lightgray", activebackground="gray",
                              troughcolor="white", highlightbackground="lightgray")
        temp_slider.pack(fill=tk.X, pady=(5, 0))
        
        # Quick preset buttons
        preset_frame = ttk.Frame(control_row1)
        preset_frame.pack(side=tk.RIGHT)
        
        ttk.Label(preset_frame, text="Quick Presets:", font=("Arial", 9)).pack()
        preset_buttons = ttk.Frame(preset_frame)
        preset_buttons.pack()
        
        presets = [
            ("Normal\n(0°C)", 0.0),
            ("Warning\n(1°C)", 1.0),
            ("Critical\n(2°C)", 2.0),
            ("Extreme\n(3°C)", 3.0)
        ]
        
        for label, value in presets:
            ttk.Button(preset_buttons, text=label, width=8,
                      command=lambda v=value: self.set_temperature_preset(v)).pack(side=tk.LEFT, padx=2)
        
        # Status row
        control_row2 = ttk.Frame(temp_frame)
        control_row2.pack(fill=tk.X)
        
        status_frame = ttk.Frame(control_row2)
        status_frame.pack(side=tk.LEFT, fill=tk.X, expand=True)
        
        ttk.Label(status_frame, text="Ocean Warming Status:", font=("Arial", 10)).pack(anchor=tk.W)
        self.warning_label = ttk.Label(status_frame, text="Temperature Offset 0.0°C - Normal baseline temperature", 
                                      foreground="green", font=("Arial", 10, "bold"))
        self.warning_label.pack(anchor=tk.W)
        
        effects_frame = ttk.Frame(control_row2)
        effects_frame.pack(side=tk.RIGHT)
        
        effects_text = "Temperature Offset Effects:\n• 0°C: Normal baseline temperature\n• 1-2°C: Ocean warming - coral stress\n• 2-3°C: Severe warming - bleaching"
        ttk.Label(effects_frame, text=effects_text, font=("Arial", 8), justify=tk.LEFT).pack()
    
    def setup_charts(self, parent):
        """Setup chart display area"""
        charts_frame = ttk.Frame(parent)
        charts_frame.pack(fill=tk.BOTH, expand=True)
        
        # Left column charts
        left_frame = ttk.Frame(charts_frame)
        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 2.5))
        
        nutrition_frame = ttk.LabelFrame(left_frame, text="Global Soil Nutrients", padding=5)
        nutrition_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 2.5))
        self.nutrition_canvas = tk.Canvas(nutrition_frame, height=200, bg="white")
        self.nutrition_canvas.pack(fill=tk.BOTH, expand=True)
        
        temp_frame = ttk.LabelFrame(left_frame, text="Temperature & Bleached Corals", padding=5)
        temp_frame.pack(fill=tk.BOTH, expand=True, pady=(2.5, 0))
        self.temp_canvas = tk.Canvas(temp_frame, height=200, bg="white")
        self.temp_canvas.pack(fill=tk.BOTH, expand=True)
        
        # Right column charts
        right_frame = ttk.Frame(charts_frame)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(2.5, 0))
        
        fish_frame = ttk.LabelFrame(right_frame, text="Fish Population", padding=5)
        fish_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 2.5))
        self.fish_canvas = tk.Canvas(fish_frame, height=200, bg="white")
        self.fish_canvas.pack(fill=tk.BOTH, expand=True)
        
        plant_frame = ttk.LabelFrame(right_frame, text="Plant Population", padding=5)
        plant_frame.pack(fill=tk.BOTH, expand=True, pady=(2.5, 0))
        self.plant_canvas = tk.Canvas(plant_frame, height=200, bg="white")
        self.plant_canvas.pack(fill=tk.BOTH, expand=True)
    
    def setup_info_panel(self, parent):
        """Setup information log panel"""
        info_frame = ttk.LabelFrame(parent, text="System Log", padding=5)
        info_frame.pack(fill=tk.X, pady=(5, 0))
        
        info_scroll_frame = ttk.Frame(info_frame)
        info_scroll_frame.pack(fill=tk.X)
        
        self.info_text = tk.Text(info_scroll_frame, height=4, wrap=tk.WORD)
        info_scrollbar = ttk.Scrollbar(info_scroll_frame, orient="vertical", command=self.info_text.yview)
        self.info_text.configure(yscrollcommand=info_scrollbar.set)
        
        self.info_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        info_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.log("Marine Ecosystem Statistics initialized successfully")
    
    def on_window_resize(self, event):
        """Handle window resize events"""
        if event.widget == self.root:
            window_width = self.root.winfo_width()
            window_height = self.root.winfo_height()
            
            available_width = window_width - 40
            available_height = window_height - 400
            
            self.canvas_width = max(520, available_width // 2)
            self.canvas_height = max(200, available_height // 2)
            
            if len(self.time_points) > 0:
                self.root.after(50, self.update_charts)
    
    def auto_start(self):
        """Start monitoring automatically"""
        self.start_monitoring()
    
    def start_monitoring(self):
        """Start data monitoring thread"""
        if self.running:
            return
        
        self.running = True
        self.update_thread = threading.Thread(target=self.data_update_loop, daemon=True)
        self.update_thread.start()
        self.log("Data monitoring thread started")
    
    def stop_monitoring(self):
        """Stop data monitoring"""
        self.running = False
        if self.update_thread:
            self.update_thread.join(timeout=1.0)
        self.log("Data monitoring stopped")
    
    def set_temperature_preset(self, temp):
        """Set temperature to preset value"""
        self.temp_var.set(temp)
        self.on_temperature_change(str(temp))
        self.log(f"Temperature preset set to {temp}°C")
    
    def on_temperature_change(self, value):
        """Handle temperature slider changes"""
        try:
            temp_value = float(value)
            self.current_temperature = temp_value
            self.temp_label.config(text=f"{temp_value:.1f}°C")
            
            # Update display color based on temperature
            if temp_value == 0.0:
                self.temp_label.config(foreground="green")
            elif temp_value < 1.5:
                self.temp_label.config(foreground="orange")
            else:
                self.temp_label.config(foreground="red")
            
            # Write temperature control file for simulation
            with open("temperature_control.tmp", "w") as f:
                f.write(f"{temp_value:.1f}")
            
            # Update status message
            if temp_value == 0.0:
                self.warning_label.config(text="Temperature Offset 0.0°C - Normal baseline temperature", 
                                         foreground="green")
            elif temp_value < 1.0:
                self.warning_label.config(text=f"Temperature Offset {temp_value:.1f}°C - Ocean warming begins", 
                                         foreground="orange")
            elif temp_value < 2.0:
                self.warning_label.config(text=f"Temperature Offset {temp_value:.1f}°C - Active coral bleaching", 
                                         foreground="red")
            else:
                self.warning_label.config(text=f"Temperature Offset {temp_value:.1f}°C - SEVERE ocean warming", 
                                         foreground="darkred")
        except Exception as e:
            self.log(f"Temperature change error: {e}")
    
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
        """Read simulation statistics from binary file"""
        try:
            if not os.path.exists(self.stats_file):
                return None, None, None, None, None
            
            # Check file age to detect simulation status
            file_age = time.time() - os.path.getmtime(self.stats_file)
            if file_age > 5.0:
                if self.simulation_connected:
                    self.log("Lost connection to simulation")
                    self.simulation_connected = False
                return None, None, None, None, None
            
            # Read binary data format: [nutrition, fish_count, plant_count, temperature, bleached_count]
            with open(self.stats_file, 'rb') as f:
                data = f.read()
                
                if len(data) == 20:  # 5 floats = 20 bytes
                    total_nutrition, fish_f, plant_f, temp, bleached_f = struct.unpack('fffff', data)
                    fish_count = int(fish_f)
                    plant_count = int(plant_f)
                    bleached_count = int(bleached_f)
                    
                    if not self.simulation_connected:
                        self.log(f"Connected to simulation: {fish_count} fish, {plant_count} plants, temp {temp:.1f}°C")
                        self.simulation_connected = True
                    
                    return total_nutrition, fish_count, plant_count, temp, bleached_count
                else:
                    self.log(f"Invalid data format: {len(data)} bytes")
                    return None, None, None, None, None
                    
        except Exception as e:
            if self.simulation_connected:
                self.log(f"Error reading stats: {e}")
            return None, None, None, None, None
    
    def data_update_loop(self):
        """Main data collection loop - only updates when data changes"""
        last_data_hash = None
        
        while self.running:
            try:
                total_nutrition, fish_count, plant_count, temperature, bleached_count = self.read_stats_file()
                
                if total_nutrition is not None:
                    # Only update if data actually changed
                    current_data_hash = hash((total_nutrition, fish_count, plant_count, temperature, bleached_count))
                    
                    if current_data_hash != last_data_hash:
                        if self.simulation_connected:
                            self.connection_label.config(text="Connected", foreground="green")
                        
                        # Store new data point
                        current_time = time.time() - self.start_time
                        self.time_points.append(current_time)
                        self.total_environmental_nutrition.append(total_nutrition)
                        self.fish_count.append(fish_count)
                        self.plant_count.append(plant_count)
                        self.temperature_data.append(temperature)
                        self.bleached_count_data.append(bleached_count)
                        
                        # Update charts only when data changes
                        self.root.after(0, self.update_charts)
                        last_data_hash = current_data_hash
                else:
                    if self.simulation_connected:
                        self.connection_label.config(text="No Data", foreground="orange")
                
                time.sleep(0.1)  # Check every 100ms, update only on changes
                
            except Exception as e:
                self.log(f"Update loop error: {e}")
                time.sleep(1.0)
    
    def update_charts(self):
        """Update all chart displays"""
        if len(self.time_points) < 2:
            return
        
        self.draw_chart(self.nutrition_canvas, "Global Soil Nutrients", 
                       self.total_environmental_nutrition, "darkgreen")
        
        self.draw_temperature_chart(self.temp_canvas, "Temperature & Bleached Corals", 
                                   self.temperature_data, self.bleached_count_data)
        
        self.draw_chart(self.fish_canvas, "Fish Population", 
                       self.fish_count, "darkblue")
        
        self.draw_chart(self.plant_canvas, "Plant Population", 
                       self.plant_count, "darkred")
    
    def draw_chart(self, canvas, title, data, color):
        """Draw standard chart with responsive layout"""
        canvas.delete("all")
        
        if not data or len(data) == 0:
            canvas.create_text(self.canvas_width//2, self.canvas_height//2, 
                             text="No Data", font=("Arial", 12), fill="gray")
            return
        
        # Calculate responsive margins
        margin = max(45, self.canvas_width // 10)
        bottom_margin = max(50, self.canvas_height // 4)
        plot_width = self.canvas_width - 2 * margin
        plot_height = self.canvas_height - margin - bottom_margin
        
        if plot_width < 100 or plot_height < 50:
            canvas.create_text(self.canvas_width//2, self.canvas_height//2, 
                             text="Window too small", font=("Arial", 10), fill="red")
            return
        
        # Data scaling
        min_val = min(data)
        max_val = max(data)
        data_range = max_val - min_val if max_val > min_val else 1
        
        # Draw axes
        canvas.create_line(margin, margin, margin, margin + plot_height, fill="black", width=2)
        canvas.create_line(margin, margin + plot_height, margin + plot_width, margin + plot_height, fill="black", width=2)
        
        # Y-axis labels
        label_x = max(15, margin - 25)
        canvas.create_text(label_x, margin, text=f"{max_val:.1f}", fill=color, font=("Arial", 8, "bold"))
        canvas.create_text(label_x, margin + plot_height//2, text=f"{(max_val+min_val)/2:.1f}", fill=color, font=("Arial", 8))
        canvas.create_text(label_x, margin + plot_height, text=f"{min_val:.1f}", fill=color, font=("Arial", 8))
        
        # X-axis time labels (start and end only)
        if len(self.time_points) > 0:
            start_time = min(self.time_points)
            end_time = max(self.time_points)
            x_label_y = margin + plot_height + 15
            
            canvas.create_text(margin, x_label_y, text=f"{start_time:.0f}s", font=("Arial", 8), fill="gray")
            canvas.create_text(margin + plot_width, x_label_y, text=f"{end_time:.0f}s", font=("Arial", 8), fill="gray")
        
        # Grid lines
        for i in range(1, 4):
            y = margin + (i * plot_height / 4)
            canvas.create_line(margin, y, margin + plot_width, y, fill="lightgray", width=1, dash=(2, 2))
        
        # Plot data line
        if len(self.time_points) > 0:
            points = []
            time_range = max(self.time_points) - min(self.time_points) if len(self.time_points) > 1 else 1
            
            for t, val in zip(self.time_points, data):
                x = margin + ((t - min(self.time_points)) / time_range) * plot_width
                y = margin + plot_height - ((val - min_val) / data_range) * plot_height
                points.extend([x, y])
            
            if len(points) >= 4:
                canvas.create_line(points, fill=color, width=2, smooth=True)
            
            # Current value indicator
            if points:
                last_x, last_y = points[-2], points[-1]
                canvas.create_oval(last_x-3, last_y-3, last_x+3, last_y+3, fill=color, outline="white", width=2)
        
        # Title
        canvas.create_text(self.canvas_width//2, 20, text=title, font=("Arial", 11, "bold"))
        
        # Current value display
        if data:
            current = data[-1]
            current_y = margin + plot_height + (bottom_margin // 2) + 5
            rect_width = min(120, self.canvas_width // 3)
            
            canvas.create_rectangle(self.canvas_width//2 - rect_width//2, current_y - 12,
                                   self.canvas_width//2 + rect_width//2, current_y + 12,
                                   fill="white", outline="darkgray", width=1)
            canvas.create_text(self.canvas_width//2, current_y, 
                             text=f"Current: {current:.1f}", font=("Arial", 10, "bold"), fill="black")
    
    def draw_temperature_chart(self, canvas, title, temp_data, bleached_data):
        """Draw temperature chart with fixed 0-3°C scale and bleached coral overlay"""
        canvas.delete("all")
        
        if not temp_data or len(temp_data) == 0:
            canvas.create_text(self.canvas_width//2, self.canvas_height//2, 
                             text="No Data", font=("Arial", 12), fill="gray")
            return
        
        # Calculate responsive margins
        margin = max(45, self.canvas_width // 10)
        bottom_margin = max(60, self.canvas_height // 3)
        plot_width = self.canvas_width - 2 * margin
        plot_height = self.canvas_height - margin - bottom_margin
        
        if plot_width < 100 or plot_height < 50:
            canvas.create_text(self.canvas_width//2, self.canvas_height//2, 
                             text="Window too small", font=("Arial", 10), fill="red")
            return
        
        # Fixed temperature scale and dynamic bleaching scale
        temp_min, temp_max = 0.0, 3.0
        temp_range = temp_max - temp_min
        
        bleached_min = 0
        bleached_max = max(bleached_data) if bleached_data and max(bleached_data) > 0 else 1
        bleached_range = bleached_max - bleached_min if bleached_max > bleached_min else 1
        
        # Draw axes
        canvas.create_line(margin, margin, margin, margin + plot_height, fill="black", width=2)
        canvas.create_line(margin, margin + plot_height, margin + plot_width, margin + plot_height, fill="black", width=2)
        
        # Temperature Y-axis labels (left side)
        label_x_left = max(15, margin - 30)
        canvas.create_text(label_x_left, margin, text="3°C", fill="orange", font=("Arial", 9, "bold"))
        canvas.create_text(label_x_left, margin + plot_height//3, text="2°C", fill="orange", font=("Arial", 8))
        canvas.create_text(label_x_left, margin + 2*plot_height//3, text="1°C", fill="orange", font=("Arial", 8))
        canvas.create_text(label_x_left, margin + plot_height, text="0°C", fill="orange", font=("Arial", 9, "bold"))
        
        # Bleached coral Y-axis labels (right side)
        if bleached_max > 0:
            label_x_right = min(self.canvas_width - 15, margin + plot_width + 30)
            canvas.create_text(label_x_right, margin, text=str(int(bleached_max)), fill="red", font=("Arial", 8, "bold"))
            canvas.create_text(label_x_right, margin + plot_height//2, text=str(int(bleached_max//2)), fill="red", font=("Arial", 8))
            canvas.create_text(label_x_right, margin + plot_height, text="0", fill="red", font=("Arial", 8))
        
        # X-axis time labels (start and end only)
        if len(self.time_points) > 0:
            start_time = min(self.time_points)
            end_time = max(self.time_points)
            x_label_y = margin + plot_height + 15
            
            canvas.create_text(margin, x_label_y, text=f"{start_time:.0f}s", font=("Arial", 8), fill="gray")
            canvas.create_text(margin + plot_width, x_label_y, text=f"{end_time:.0f}s", font=("Arial", 8), fill="gray")
        
        # Grid lines
        for i in range(1, 4):
            y = margin + (i * plot_height / 4)
            canvas.create_line(margin, y, margin + plot_width, y, fill="lightgray", width=1, dash=(2, 2))
        
        # Plot data
        if len(self.time_points) > 0:
            time_range = max(self.time_points) - min(self.time_points) if len(self.time_points) > 1 else 1
            
            # Temperature line (orange)
            temp_points = []
            for t, temp in zip(self.time_points, temp_data):
                x = margin + ((t - min(self.time_points)) / time_range) * plot_width
                y = margin + plot_height - ((temp - temp_min) / temp_range) * plot_height
                temp_points.extend([x, y])
            
            if len(temp_points) >= 4:
                canvas.create_line(temp_points, fill="orange", width=3, smooth=True)
            
            # Bleached coral line (red, dashed)
            if bleached_data and len(bleached_data) > 0 and bleached_max > 0:
                bleached_points = []
                for t, bleached in zip(self.time_points, bleached_data):
                    x = margin + ((t - min(self.time_points)) / time_range) * plot_width
                    y = margin + plot_height - ((bleached - bleached_min) / bleached_range) * plot_height
                    bleached_points.extend([x, y])
                
                if len(bleached_points) >= 4:
                    canvas.create_line(bleached_points, fill="red", width=2, smooth=True, dash=(5, 5))
            
            # Current value indicators
            if temp_points:
                last_x, last_y = temp_points[-2], temp_points[-1]
                canvas.create_oval(last_x-4, last_y-4, last_x+4, last_y+4, fill="orange", outline="white", width=2)
            
            if bleached_data and len(bleached_data) > 0 and len(bleached_points) >= 2:
                last_x, last_y = bleached_points[-2], bleached_points[-1]
                canvas.create_oval(last_x-3, last_y-3, last_x+3, last_y+3, fill="red", outline="white", width=2)
        
        # Title
        canvas.create_text(self.canvas_width//2, 20, text=title, font=("Arial", 11, "bold"))
        
        # Current values display
        if temp_data:
            current_temp = temp_data[-1]
            current_bleached = bleached_data[-1] if bleached_data and len(bleached_data) > 0 else 0
            text = f"Current: {current_temp:.1f}°C, Bleached {int(current_bleached)}"
            
            current_y = margin + plot_height + (bottom_margin // 2) + 10
            rect_width = min(240, self.canvas_width - 20)
            
            canvas.create_rectangle(self.canvas_width//2 - rect_width//2, current_y - 12,
                                   self.canvas_width//2 + rect_width//2, current_y + 12,
                                   fill="white", outline="darkgray", width=1)
            canvas.create_text(self.canvas_width//2, current_y, 
                             text=text, font=("Arial", 10, "bold"), fill="black")
    
    def on_closing(self):
        """Handle application shutdown"""
        self.log("Shutting down statistics monitor...")
        self.stop_monitoring()
        self.root.destroy()
    
    def run(self):
        """Start the main application loop"""
        print("GUI initialized successfully")
        print("Temperature control system ready")
        print("Chart rendering system active")
        print("Data monitoring thread started")
        print("System ready - Statistics window active")
        print("WARNING: This statistics window will close when the terminal is closed!")
        
        try:
            self.root.mainloop()
        except KeyboardInterrupt:
            self.log("Interrupted by user")
        finally:
            self.stop_monitoring()

if __name__ == "__main__":
    try:
        stats = EcosystemStats()
        stats.run()
    except Exception as e:
        print(f"Error starting ecosystem statistics: {e}")
        import traceback
        traceback.print_exc()