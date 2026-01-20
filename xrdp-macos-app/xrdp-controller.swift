//
//  xrdp-controller.swift
//  xrdp Remote Desktop
//
//  Copyright (C) 2026 Neutrinos Software Corporation
//  Some portions Classify®
//

import SwiftUI
import UserNotifications

// MARK: - Session Model
@Observable
@MainActor
class XRDPSession: Identifiable {
    let id = UUID()
    var sessionId: String = ""
    var username: String = ""
    var ipAddress: String = ""
    var protocolType: String = "RDP"
    var encryption: String = "TLS 1.3 ChaCha20-Poly1305"
    var pid: Int = 0
    var connectedAt: Date = Date()
}

// MARK: - App State
@Observable
@MainActor
class XRDPAppState {
    var activeSessions: [XRDPSession] = []
    var connectionCount: Int = 0

    var connectionsText: String {
        "Active Connections: \(connectionCount)"
    }
}

// MARK: - Server Manager
@Observable
@MainActor
class XRDPServerManager {
    var appState = XRDPAppState()
    private var monitorTask: Task<Void, Never>?
    private var logMonitorTask: Task<Void, Never>?
    private var xrdpTask: Process?
    private var sesmanTask: Process?
    private var previousSessionPIDs: Set<Int> = []
    private var lastLogPosition: UInt64 = 0
    private var connectionAttempts: Int = 0
    private var failedConnections: Int = 0
    private var hasAutoStarted = false

    // Computed property - no background updates, only checked when accessed
    var isServerRunning: Bool {
        xrdpTask?.isRunning ?? false
    }

    var statusText: String {
        isServerRunning ? "XRDP Server: Running" : "XRDP Server: Stopped"
    }

    init() {
        // Do everything in background to never block UI
        Task.detached { @MainActor @Sendable [weak self] in
            guard let self = self else { return }

            self.requestNotificationPermission()
            self.startLogMonitoring()

            try? await Task.sleep(for: .seconds(1))
            if !self.hasAutoStarted {
                self.hasAutoStarted = true
                self.killExistingProcesses()
                try? await Task.sleep(for: .seconds(0.5))
                self.startServer()
            }
        }
    }

    private func requestNotificationPermission() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { granted, error in
            if granted {
                print("Notification permission granted")
            } else if let error = error {
                print("Notification permission error: \(error)")
            }
        }
    }

    func startServer() {
        Task {
            await startServerAsync()
        }
    }

    private func startServerAsync() async {
        guard xrdpTask == nil || !(xrdpTask?.isRunning ?? false) else { return }

        print("Starting xrdp server...")

        let helpersDir = Bundle.main.bundlePath + "/Contents/Helpers"
        let resourcesDir = Bundle.main.resourcePath ?? ""

        // Environment
        var env = ProcessInfo.processInfo.environment
        env["XRDP_RUNTIME_DIR"] = Bundle.main.bundlePath + "/Contents/run"

        // Start xrdp-sesman
        sesmanTask = Process()
        sesmanTask?.executableURL = URL(fileURLWithPath: "\(helpersDir)/xrdp-sesman")
        sesmanTask?.arguments = ["--nodaemon", "-c", "\(resourcesDir)/etc/xrdp/sesman.ini"]
        sesmanTask?.environment = env

        do {
            try sesmanTask?.run()
            print("Started xrdp-sesman (PID: \(sesmanTask?.processIdentifier ?? 0))")
        } catch {
            print("Failed to start xrdp-sesman: \(error)")
            return
        }

        try? await Task.sleep(for: .seconds(0.5))

        // Start xrdp
        xrdpTask = Process()
        xrdpTask?.executableURL = URL(fileURLWithPath: "\(helpersDir)/xrdp")
        xrdpTask?.arguments = ["--nodaemon", "--port", "3389", "-c", "\(resourcesDir)/etc/xrdp/xrdp.ini"]
        xrdpTask?.environment = env

        // Set up pipes to capture output
        let outputPipe = Pipe()
        let errorPipe = Pipe()
        xrdpTask?.standardOutput = outputPipe
        xrdpTask?.standardError = errorPipe

        // Monitor termination
        xrdpTask?.terminationHandler = { [weak self] process in
            print("xrdp terminated with status: \(process.terminationStatus)")

            var errorDetails = "Exit code: \(process.terminationStatus)"
            if let errorData = try? errorPipe.fileHandleForReading.readToEnd(),
               let errorString = String(data: errorData, encoding: .utf8), !errorString.isEmpty {
                print("xrdp error output: \(errorString)")
                errorDetails += "\n\(errorString)"
            }

            // Send crash notification
            self?.sendNotification(
                title: "⚠️ xrdp Server Crashed",
                body: "Server terminated unexpectedly. \(errorDetails)",
                sound: true
            )
            // No need to update state - isServerRunning is computed from xrdpTask.isRunning
        }

        do {
            try xrdpTask?.run()
            print("Started xrdp (PID: \(xrdpTask?.processIdentifier ?? 0))")
            print("Note: xrdp-chansrv will be started automatically by sesman when a user connects")

            // Give it a moment to start
            try? await Task.sleep(for: .seconds(0.5))

            // Verify it's still running
            if xrdpTask?.isRunning == true {
                print("xrdp daemon is running successfully")
                // No need to update state - isServerRunning is computed from xrdpTask.isRunning
                startConnectionMonitoring()
            } else {
                print("xrdp daemon failed to start or exited immediately")
                if let errorData = try? errorPipe.fileHandleForReading.readToEnd(),
                   let errorString = String(data: errorData, encoding: .utf8) {
                    print("Error output: \(errorString)")
                }
                sesmanTask?.terminate()
            }
        } catch {
            print("Failed to start xrdp: \(error)")
            sesmanTask?.terminate()
        }
    }

    func stopServer() {
        print("Stopping xrdp server...")

        stopConnectionMonitoring()

        if let xrdp = xrdpTask, xrdp.isRunning {
            print("Terminating xrdp daemon (PID: \(xrdp.processIdentifier))")
            xrdp.terminate()
        }
        xrdpTask = nil

        if let sesman = sesmanTask, sesman.isRunning {
            print("Terminating xrdp-sesman (PID: \(sesman.processIdentifier))")
            sesman.terminate()
        }
        sesmanTask = nil

        killExistingProcesses()
        // No need to update state - isServerRunning is computed from xrdpTask.isRunning
    }

    deinit {
        print("XRDPServerManager deinit - this should NOT happen while app is running!")
        // Deinit runs on non-isolated context, can't access actor-isolated properties
    }

    func killExistingProcesses() {
        Task {
            await killExistingProcessesAsync()
        }
    }

    private func killExistingProcessesAsync() async {
        print("Checking for existing xrdp server processes...")

        let helpersDir = Bundle.main.bundlePath + "/Contents/Helpers"

        for processName in ["xrdp", "xrdp-sesman", "xrdp-chansrv"] {
            let task = Process()
            task.launchPath = "/usr/bin/pkill"
            task.arguments = ["-9", "-f", "\(helpersDir)/\(processName)"]

            try? task.run()
            task.waitUntilExit()
        }

        try? await Task.sleep(for: .seconds(0.3))
        print("Cleanup complete")
    }

    private func startConnectionMonitoring() {
        // Disabled connection monitoring to prevent menu hangs
        // The issue is that updating observable state while menu is open causes SwiftUI to hang
        monitorTask?.cancel()
        monitorTask = nil
    }

    private func stopConnectionMonitoring() {
        monitorTask?.cancel()
        monitorTask = nil
        previousSessionPIDs.removeAll()
        appState.activeSessions.removeAll()
        appState.connectionCount = 0
    }

    private func startLogMonitoring() {
        logMonitorTask?.cancel()
        logMonitorTask = Task { [weak self] in
            // Get initial file size
            let logPath = NSHomeDirectory() + "/Library/Logs/xrdp/xrdp.log"
            if let attrs = try? FileManager.default.attributesOfItem(atPath: logPath),
               let fileSize = attrs[.size] as? UInt64 {
                self?.lastLogPosition = fileSize
            }

            while !Task.isCancelled {
                self?.checkLogForErrors()
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    private func checkLogForErrors() {
        let logPath = NSHomeDirectory() + "/Library/Logs/xrdp/xrdp.log"

        guard let fileHandle = FileHandle(forReadingAtPath: logPath) else { return }
        defer { try? fileHandle.close() }

        // Get current file size
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: logPath),
              let currentSize = attrs[.size] as? UInt64 else { return }

        // Only read new content
        if currentSize <= lastLogPosition { return }

        fileHandle.seek(toFileOffset: lastLogPosition)
        let newData = fileHandle.readDataToEndOfFile()
        lastLogPosition = currentSize

        guard let newContent = String(data: newData, encoding: .utf8) else { return }

        // Parse for errors and connection attempts
        let lines = newContent.components(separatedBy: "\n")
        for line in lines {
            if line.contains("[ERROR]") {
                parseError(line)
            } else if line.contains("TLS handshake") || line.contains("connection") {
                parseConnectionEvent(line)
            }
        }
    }

    private func parseError(_ line: String) {
        print("ERROR detected: \(line)")

        // Extract error details
        if line.contains("Decryption failed") {
            failedConnections += 1
            sendNotification(
                title: "🔒 TLS Decryption Error",
                body: "Connection failed: Bad MAC after handshake. Client may not support TLS 1.3 properly. Failed: \(failedConnections)",
                sound: false
            )
        } else if line.contains("TLS handshake failed") {
            failedConnections += 1
            sendNotification(
                title: "🔒 TLS Handshake Failed",
                body: "Client TLS handshake error. Total failures: \(failedConnections)",
                sound: false
            )
        } else if line.contains("libxrdp_force_read: header read error") {
            sendNotification(
                title: "📡 Protocol Error",
                body: "RDP header read failed after TLS. Connection dropped.",
                sound: false
            )
        } else if line.contains("xrdp_mcs_incoming failed") {
            sendNotification(
                title: "🔌 MCS Connection Failed",
                body: "Multi-Channel Service connection failed. RDP protocol layer error.",
                sound: false
            )
        }
    }

    private func parseConnectionEvent(_ line: String) {
        if line.contains("new connection") || line.contains("starting xrdp") {
            connectionAttempts += 1
            print("Connection attempt #\(connectionAttempts)")
        }
    }

    private func updateConnectionsAsync() async {
        let task = Process()
        task.launchPath = "/bin/ps"
        task.arguments = ["aux"]

        let pipe = Pipe()
        task.standardOutput = pipe

        do {
            try task.run()
            task.waitUntilExit()

            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            if let output = String(data: data, encoding: .utf8) {
                await parseConnectionInfo(output)
            }
        } catch {
            print("Failed to get connection info: \(error)")
        }
    }

    private func parseConnectionInfo(_ psOutput: String) async {
        var newSessions: [XRDPSession] = []

        let lines = psOutput.components(separatedBy: "\n")
        for line in lines {
            if line.contains("xrdp-chansrv") && !line.contains("grep") {
                if let session = parseSessionFromProcessLine(line) {
                    newSessions.append(session)
                }
            }
        }

        // Detect new connections
        let currentPIDs = Set(newSessions.map { $0.pid })
        let newPIDs = currentPIDs.subtracting(previousSessionPIDs)
        let disconnectedPIDs = previousSessionPIDs.subtracting(currentPIDs)

        // Notify for new connections
        for pid in newPIDs {
            if let session = newSessions.first(where: { $0.pid == pid }) {
                sendNotification(
                    title: "New Connection",
                    body: "\(session.username) connected from \(session.ipAddress)",
                    sound: true
                )
            }
        }

        // Notify for disconnections
        for pid in disconnectedPIDs {
            if let session = appState.activeSessions.first(where: { $0.pid == pid }) {
                sendNotification(
                    title: "User Disconnected",
                    body: "\(session.username) from \(session.ipAddress) has disconnected",
                    sound: false
                )
            }
        }

        // Only update state if it actually changed to avoid unnecessary re-renders
        let hasChanged = currentPIDs != previousSessionPIDs || newSessions.count != appState.connectionCount
        if hasChanged {
            previousSessionPIDs = currentPIDs
            appState.activeSessions = newSessions
            appState.connectionCount = newSessions.count
        }
    }

    nonisolated private func sendNotification(title: String, body: String, sound: Bool) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        if sound {
            content.sound = .default
        }

        let request = UNNotificationRequest(
            identifier: UUID().uuidString,
            content: content,
            trigger: nil
        )

        UNUserNotificationCenter.current().add(request) { error in
            if let error = error {
                print("Notification error: \(error)")
            }
        }
    }

    private func parseSessionFromProcessLine(_ line: String) -> XRDPSession? {
        let components = line.components(separatedBy: .whitespaces).filter { !$0.isEmpty }
        guard components.count >= 2 else { return nil }

        let session = XRDPSession()
        session.username = components[0]
        session.pid = Int(components[1]) ?? 0

        getIPAddress(forPID: session.pid, session: session)

        return session
    }

    private func getIPAddress(forPID pid: Int, session: XRDPSession) {
        let task = Process()
        task.launchPath = "/usr/sbin/lsof"
        task.arguments = ["-p", "\(pid)", "-n", "-P", "-iTCP"]

        let pipe = Pipe()
        task.standardOutput = pipe
        task.standardError = Pipe()

        do {
            try task.run()
            task.waitUntilExit()

            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            if let output = String(data: data, encoding: .utf8) {
                let lines = output.components(separatedBy: "\n")
                for line in lines {
                    if line.contains("->") {
                        if let arrowRange = line.range(of: "->") {
                            let afterArrow = String(line[arrowRange.upperBound...])
                            let parts = afterArrow.components(separatedBy: ":")
                            if !parts.isEmpty {
                                session.ipAddress = parts[0]
                                return
                            }
                        }
                    }
                }
            }
        } catch {
            // Ignore
        }

        session.ipAddress = "127.0.0.1"
    }

    func disconnectSession(_ session: XRDPSession) {
        Task {
            let task = Process()
            task.launchPath = "/bin/kill"
            task.arguments = ["-9", "\(session.pid)"]

            do {
                try task.run()
                task.waitUntilExit()
                print("Disconnected session for \(session.username) (PID: \(session.pid))")

                // Send notification
                sendNotification(
                    title: "Session Terminated",
                    body: "Manually disconnected \(session.username) from \(session.ipAddress)",
                    sound: false
                )

                // Force immediate update
                await updateConnectionsAsync()
            } catch {
                print("Failed to disconnect session: \(error)")
            }
        }
    }
}

// MARK: - SwiftUI Views

struct MenuBarView: View {
    @Bindable var serverManager: XRDPServerManager

    var body: some View {
        VStack(spacing: 0) {
            // Status
            Text(serverManager.statusText)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)

            Divider()

            // Connections
            Text("Active Connections: \(serverManager.appState.connectionCount)")
                .padding(.horizontal, 12)
                .padding(.vertical, 6)

            if serverManager.appState.connectionCount == 0 {
                Text("No active connections")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 12)
                    .padding(.bottom, 6)
            } else {
                ForEach(serverManager.appState.activeSessions) { session in
                    VStack(alignment: .leading, spacing: 4) {
                        Text("\(session.username) from \(session.ipAddress)")
                            .font(.subheadline)
                        Text("Protocol: \(session.protocolType) • Encryption: \(session.encryption)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Text("PID: \(session.pid)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Button("Disconnect Session") {
                            disconnectWithConfirmation(session)
                        }
                        .buttonStyle(.borderedProminent)
                        .controlSize(.small)
                    }
                    .padding(.horizontal, 12)
                    .padding(.vertical, 6)

                    if session.id != serverManager.appState.activeSessions.last?.id {
                        Divider()
                    }
                }
            }

            Divider()

            // Start/Stop
            Button("Start Server") {
                serverManager.startServer()
            }
            .disabled(serverManager.isServerRunning)

            Button("Stop Server") {
                serverManager.stopServer()
            }
            .disabled(!serverManager.isServerRunning)

            Divider()

            // About
            Button("About XRDP...") {
                showAboutDialog()
            }

            Divider()

            // Quit
            Button("Quit") {
                serverManager.stopServer()
                NSApp.terminate(nil)
            }
            .keyboardShortcut("q")
        }
    }

    private func showAboutDialog() {
        let alert = NSAlert()
        alert.messageText = "⚛ XRDP for macOS"
        alert.informativeText = """
        XRDP - Open Source Remote Desktop Protocol Server
        Version: 1.0.0

        Built with NeutrinoTLS - Pure C TLS 1.3 Implementation
        Using ChaCha20-Poly1305 AEAD Encryption

        ACKNOWLEDGEMENTS

        XRDP Project:
          Copyright © 2004-2024 Jay Sorg and all contributors
          Licensed under Apache License 2.0

        NeutrinoTLS:
          Pure C implementation of TLS 1.3 (RFC 8446)
          ChaCha20-Poly1305 (RFC 7539)
          X25519 Key Exchange (RFC 7748)
          HKDF-SHA256 (RFC 5869)

        OpenSSL:
          Copyright © 1998-2024 The OpenSSL Project
          Licensed under Apache License 2.0

        macOS Integration:
          Copyright © 2026 Neutrinos Software Corporation
          Some portions Classify®

        LICENSES

        XRDP: Apache License 2.0
        OpenSSL: Apache License 2.0
        NeutrinoTLS: Apache License 2.0

        Full license text available at:
        https://www.apache.org/licenses/LICENSE-2.0
        """
        alert.addButton(withTitle: "OK")
        alert.addButton(withTitle: "View on GitHub")

        if alert.runModal() == .alertSecondButtonReturn {
            if let url = URL(string: "https://github.com/neutrinolabs/xrdp") {
                NSWorkspace.shared.open(url)
            }
        }
    }

    private func disconnectWithConfirmation(_ session: XRDPSession) {
        let alert = NSAlert()
        alert.messageText = "Disconnect User?"
        alert.informativeText = "Are you sure you want to disconnect \(session.username) from \(session.ipAddress)?"
        alert.addButton(withTitle: "Disconnect")
        alert.addButton(withTitle: "Cancel")

        if alert.runModal() == .alertFirstButtonReturn {
            serverManager.disconnectSession(session)
        }
    }
}

// MARK: - App

@main
struct XRDPApp: App {
    @State private var serverManager = XRDPServerManager()

    var body: some Scene {
        MenuBarExtra {
            MenuBarView(serverManager: serverManager)
        } label: {
            if #available(macOS 11.0, *) {
                Image(systemName: "atom")
            } else {
                Text("⚛")
            }
        }
        .menuBarExtraStyle(.menu)
    }
}
