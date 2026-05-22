/**
 * Kotlin/Java bindings for mobile optimization features
 * 
 * Add these methods to your NativeLib class in Android
 */

// In your NativeLib.kt or NativeLib.java file:

package com.droid.ethervox_core

object NativeLib {
    
    // Existing methods...
    external fun loadGovernorModel(modelPath: String): Boolean
    external fun unloadGovernorModel(): Boolean
    
    // NEW: Mobile Optimization Features
    
    /**
     * Load Governor model in MINIMAL MODE for fast mobile startup
     * 
     * This mode uses a brief system prompt (~50 tokens vs ~1200)
     * resulting in 90% faster loading on mobile devices.
     * 
     * Trade-off: Tools are disabled (no memory, file operations, etc.)
     * Use for quick voice queries that don't need tool execution.
     * 
     * @param modelPath Path to GGUF model file
     * @return true if loaded successfully, false otherwise
     */
    external fun loadGovernorModelMinimal(modelPath: String): Boolean
    
    /**
     * Enable or disable SECRET MODE (privacy mode)
     * 
     * When enabled:
     * - Conversations are NOT saved to memory/disk
     * - LLM continues working normally
     * - Tool calls return success but skip actual storage
     * 
     * Use cases:
     * - Sensitive conversations (medical, financial, personal)
     * - Temporary queries user doesn't want logged
     * - Public demos
     * 
     * @param enabled true to enable secret mode, false for normal logging
     */
    external fun setPrivacyMode(enabled: Boolean)
    
    // Load native library
    init {
        System.loadLibrary("ethervox_core")
    }
}

/**
 * Example Android UI integration
 */

class MainActivity : AppCompatActivity() {
    
    private var isSecretModeEnabled = false
    private var isMinimalModeEnabled = false
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        setupPrivacyToggle()
        setupModelLoadingOptions()
    }
    
    private fun setupPrivacyToggle() {
        val secretModeSwitch: SwitchCompat = findViewById(R.id.secretModeSwitch)
        
        secretModeSwitch.setOnCheckedChangeListener { _, isChecked ->
            isSecretModeEnabled = isChecked
            NativeLib.setPrivacyMode(isChecked)
            
            if (isChecked) {
                // Show privacy indicator
                Toast.makeText(
                    this,
                    "🔒 Secret mode enabled - conversations won't be saved",
                    Toast.LENGTH_LONG
                ).show()
                
                // Update UI to show privacy status
                findViewById<TextView>(R.id.statusText).apply {
                    text = "SECRET MODE ACTIVE"
                    setTextColor(Color.RED)
                }
            } else {
                Toast.makeText(
                    this,
                    "Normal mode - conversations will be saved",
                    Toast.LENGTH_SHORT
                ).show()
                
                findViewById<TextView>(R.id.statusText).apply {
                    text = "Normal Mode"
                    setTextColor(Color.GREEN)
                }
            }
        }
    }
    
    private fun setupModelLoadingOptions() {
        val minimalModeCheckbox: CheckBox = findViewById(R.id.minimalModeCheckbox)
        val loadModelButton: Button = findViewById(R.id.loadModelButton)
        
        minimalModeCheckbox.setOnCheckedChangeListener { _, isChecked ->
            isMinimalModeEnabled = isChecked
            
            if (isChecked) {
                Toast.makeText(
                    this,
                    "⚡ Minimal mode - fast loading, tools disabled",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
        
        loadModelButton.setOnClickListener {
            val modelPath = getModelPath() // Your model path logic
            
            val success = if (isMinimalModeEnabled) {
                // Fast loading for quick queries
                NativeLib.loadGovernorModelMinimal(modelPath)
            } else {
                // Full mode with all tools
                NativeLib.loadGovernorModel(modelPath)
            }
            
            if (success) {
                Toast.makeText(
                    this,
                    if (isMinimalModeEnabled) {
                        "Model loaded (minimal mode - fast)"
                    } else {
                        "Model loaded (full mode - all tools)"
                    },
                    Toast.LENGTH_SHORT
                ).show()
            } else {
                Toast.makeText(this, "Failed to load model", Toast.LENGTH_SHORT).show()
            }
        }
    }
    
    /**
     * Show privacy indicator in notification bar
     */
    private fun showSecretModeNotification() {
        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_privacy)
            .setContentTitle("EthervoxAI - Secret Mode")
            .setContentText("Conversations are not being saved")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setOngoing(true)
            .build()
            
        notificationManager.notify(SECRET_MODE_NOTIFICATION_ID, notification)
    }
    
    /**
     * Recommended: Auto-disable secret mode after X minutes
     */
    private fun scheduleSecretModeTimeout() {
        Handler(Looper.getMainLooper()).postDelayed({
            if (isSecretModeEnabled) {
                findViewById<SwitchCompat>(R.id.secretModeSwitch).isChecked = false
                Toast.makeText(
                    this,
                    "Secret mode auto-disabled after 30 minutes",
                    Toast.LENGTH_LONG
                ).show()
            }
        }, 30 * 60 * 1000) // 30 minutes
    }
}

/**
 * Example layout.xml
 */
/*
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical"
    android:padding="16dp">
    
    <!-- Privacy Toggle -->
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="horizontal"
        android:gravity="center_vertical">
        
        <TextView
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="Secret Mode (No Logging)"
            android:textSize="16sp"/>
        
        <androidx.appcompat.widget.SwitchCompat
            android:id="@+id/secretModeSwitch"
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"/>
    </LinearLayout>
    
    <!-- Status Indicator -->
    <TextView
        android:id="@+id/statusText"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:text="Normal Mode"
        android:textColor="@color/green"
        android:textSize="14sp"
        android:layout_marginTop="8dp"/>
    
    <!-- Model Loading Options -->
    <CheckBox
        android:id="@+id/minimalModeCheckbox"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:text="Fast Loading (Minimal Mode)"
        android:layout_marginTop="16dp"/>
    
    <Button
        android:id="@+id/loadModelButton"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:text="Load Model"
        android:layout_marginTop="8dp"/>
        
</LinearLayout>
*/

/**
 * Recommended Settings Screen
 */
class SettingsFragment : PreferenceFragmentCompat() {
    
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
        
        // Privacy mode preference
        findPreference<SwitchPreferenceCompat>("secret_mode")?.apply {
            setOnPreferenceChangeListener { _, newValue ->
                val enabled = newValue as Boolean
                NativeLib.setPrivacyMode(enabled)
                
                // Save preference
                PreferenceManager.getDefaultSharedPreferences(requireContext())
                    .edit()
                    .putBoolean("secret_mode", enabled)
                    .apply()
                
                true
            }
        }
        
        // Minimal mode preference
        findPreference<SwitchPreferenceCompat>("minimal_mode")?.apply {
            setOnPreferenceChangeListener { _, newValue ->
                val enabled = newValue as Boolean
                
                // Save preference for next model load
                PreferenceManager.getDefaultSharedPreferences(requireContext())
                    .edit()
                    .putBoolean("minimal_mode", enabled)
                    .apply()
                
                // Show info
                Toast.makeText(
                    requireContext(),
                    if (enabled) {
                        "Next model load will use minimal mode (fast)"
                    } else {
                        "Next model load will use full mode (all tools)"
                    },
                    Toast.LENGTH_SHORT
                ).show()
                
                true
            }
        }
    }
}

/**
 * Performance Monitoring
 */
class ModelLoadingMetrics {
    
    fun measureLoadTime(modelPath: String, useMinimalMode: Boolean): Long {
        val startTime = System.currentTimeMillis()
        
        val success = if (useMinimalMode) {
            NativeLib.loadGovernorModelMinimal(modelPath)
        } else {
            NativeLib.loadGovernorModel(modelPath)
        }
        
        val loadTime = System.currentTimeMillis() - startTime
        
        Log.i(TAG, "Model load time: ${loadTime}ms (minimal=$useMinimalMode, success=$success)")
        
        // Log to analytics
        FirebaseAnalytics.getInstance(context).logEvent("model_load") {
            param("load_time_ms", loadTime)
            param("minimal_mode", useMinimalMode)
            param("success", success)
        }
        
        return loadTime
    }
}

/**
 * Best Practices
 */
/*
1. SECRET MODE:
   - Show clear UI indicator when active
   - Consider auto-timeout (30 min recommended)
   - Show notification in status bar
   - Warn user before disabling
   
2. MINIMAL MODE:
   - Use for quick voice queries on budget devices
   - Show "tools disabled" indicator
   - Offer option to reload in full mode if user needs tools
   - Measure and log performance improvements
   
3. COMBINED MODES:
   - Minimal + Secret = ultra-private quick queries
   - Ideal for public demos or sensitive situations
   
4. USER EDUCATION:
   - Explain trade-offs clearly
   - Show performance benefits (90% faster loading)
   - Clarify what's disabled in minimal mode
*/
