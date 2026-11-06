plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "dev.periscope.lens"
    compileSdk = 34

    defaultConfig {
        applicationId = "dev.periscope.lens"
        minSdk = 26          // Camera2 high-speed + USB gadget APIs
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    // ONNX Runtime Mobile: first NPU route via the NNAPI execution provider.
    implementation("com.microsoft.onnxruntime:onnxruntime-android:1.19.2")
}
