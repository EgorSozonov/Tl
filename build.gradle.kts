import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

plugins {
    application
    kotlin("jvm") version "1.7.10"
}

group = "tech.sozonov"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

buildDir = File("./_bin")

application {
    mainClass.set("io.ktor.server.netty.EngineMain")
}

sourceSets {
    main {
        java {
            this.setSrcDirs(mutableListOf("source"))
        }
        resources {
            this.setSrcDirs(mutableListOf("config", "resources"))
        }
    }
    test {
        java {
            this.setSrcDirs(mutableListOf("test"))
        }
    }
}
