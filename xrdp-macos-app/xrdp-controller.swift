//
//  xrdp-controller.swift
//  xrdp Remote Desktop
//
//  Copyright (C) 2026 Neutrinos Software Corporation
//  Some portions Classify®
//

import SwiftUI
import UserNotifications

// MARK: - Notification Model
@Observable
@MainActor
class XRDPNotification: Identifiable {
    let id = UUID()
    var title: String
    var body: String
    var timestamp: Date
    var category: NotificationCategory
    var debugInfo: String = ""

    enum NotificationCategory: String {
        case connection = "Connection"
        case disconnection = "Disconnection"
        case tlsError = "TLS Error"
        case protocolError = "Protocol Error"
        case serverCrash = "Server Crash"
        case other = "Other"
    }

    init(title: String, body: String, category: NotificationCategory = .other, debugInfo: String = "") {
        self.title = title
        self.body = body
        self.timestamp = Date()
        self.category = category
        self.debugInfo = debugInfo
    }

    var troubleshootingSteps: String {
        switch category {
        case .tlsError:
            return """
            TROUBLESHOOTING STEPS:
            1. Verify client supports TLS 1.3
            2. Check if client uses ChaCha20-Poly1305 cipher
            3. Try updating your RDP client to latest version
            4. On Windows: Use Microsoft Remote Desktop (not mstsc.exe)
            5. On macOS: Use Microsoft Remote Desktop from App Store

            TECHNICAL DETAILS:
            - Server uses NeutrinoTLS 1.3
            - Cipher: TLS_CHACHA20_POLY1305_SHA256
            - Key Exchange: X25519 (Curve25519 ECDH)

            COMMON CAUSES:
            - Client doesn't support TLS 1.3
            - Client using older TLS version (1.2 or below)
            - Incompatible cipher suite negotiation
            """
        case .protocolError:
            return """
            TROUBLESHOOTING STEPS:
            1. Check network connectivity
            2. Verify firewall allows port 3389
            3. Restart XRDP server
            4. Check client RDP version compatibility
            5. Review server logs for details

            TECHNICAL DETAILS:
            - Protocol: RDP 8.0+
            - Port: 3389 (TCP)
            - Encryption: TLS 1.3

            COMMON CAUSES:
            - Network interruption during connection
            - Client sent malformed RDP packet
            - TLS connection dropped unexpectedly
            """
        case .serverCrash:
            return """
            TROUBLESHOOTING STEPS:
            1. Check system logs for crash details
            2. Restart the XRDP server
            3. Verify all helper processes are running
            4. Check system resources (CPU, memory)
            5. Report issue if crash persists

            TECHNICAL DETAILS:
            - Components: xrdp, xrdp-sesman, xrdp-chansrv
            - Runtime Dir: ~/Library/Application Support/xrdp
            - Logs: ~/Library/Logs/xrdp/

            COMMON CAUSES:
            - Out of memory
            - Segmentation fault in xrdp daemon
            - Missing or corrupted config files
            """
        case .connection:
            return """
            INFORMATION:
            A new RDP connection was established successfully.

            CONNECTION DETAILS:
            - Protocol: RDP over TLS 1.3
            - Encryption: ChaCha20-Poly1305
            - Authentication: System credentials

            SECURITY:
            - Connection is encrypted end-to-end
            - Server uses Developer ID signed certificate
            - All traffic secured via TLS 1.3
            """
        case .disconnection:
            return """
            INFORMATION:
            An RDP session was disconnected.

            NORMAL DISCONNECTION:
            - User logged out
            - Client closed connection
            - Session timeout

            IF UNEXPECTED:
            1. Check network stability
            2. Verify client didn't crash
            3. Review server logs
            """
        case .other:
            return """
            TROUBLESHOOTING STEPS:
            1. Review the notification details above
            2. Check server logs at ~/Library/Logs/xrdp/
            3. Restart XRDP server if needed
            4. Contact support if issue persists
            """
        }
    }
}

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
    var recentNotifications: [XRDPNotification] = []
    private let maxNotifications = 50  // Keep last 50 notifications

    var connectionsText: String {
        "Active Connections: \(connectionCount)"
    }

    func addNotification(_ notification: XRDPNotification) {
        recentNotifications.insert(notification, at: 0)
        if recentNotifications.count > maxNotifications {
            recentNotifications.removeLast()
        }
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
                sound: true,
                category: .serverCrash,
                debugInfo: "Exit code: \(process.terminationStatus)\nPID: \(process.processIdentifier)"
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
                sound: false,
                category: .tlsError,
                debugInfo: line
            )
        } else if line.contains("TLS handshake failed") {
            failedConnections += 1
            sendNotification(
                title: "🔒 TLS Handshake Failed",
                body: "Client TLS handshake error. Total failures: \(failedConnections)",
                sound: false,
                category: .tlsError,
                debugInfo: line
            )
        } else if line.contains("libxrdp_force_read: header read error") {
            sendNotification(
                title: "📡 Protocol Error",
                body: "RDP header read failed after TLS. Connection dropped.",
                sound: false,
                category: .protocolError,
                debugInfo: line
            )
        } else if line.contains("xrdp_mcs_incoming failed") {
            sendNotification(
                title: "🔌 MCS Connection Failed",
                body: "Multi-Channel Service connection failed. RDP protocol layer error.",
                sound: false,
                category: .protocolError,
                debugInfo: line
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
                    sound: true,
                    category: .connection,
                    debugInfo: "PID: \(session.pid)\nProtocol: \(session.protocolType)\nEncryption: \(session.encryption)"
                )
            }
        }

        // Notify for disconnections
        for pid in disconnectedPIDs {
            if let session = appState.activeSessions.first(where: { $0.pid == pid }) {
                sendNotification(
                    title: "User Disconnected",
                    body: "\(session.username) from \(session.ipAddress) has disconnected",
                    sound: false,
                    category: .disconnection,
                    debugInfo: "PID: \(session.pid)\nDuration: \(Date().timeIntervalSince(session.connectedAt)) seconds"
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

    nonisolated private func sendNotification(title: String, body: String, sound: Bool, category: XRDPNotification.NotificationCategory = .other, debugInfo: String = "") {
        // Format timestamp
        let formatter = DateFormatter()
        formatter.dateFormat = "MMM d, yyyy 'at' h:mm:ss a"
        let timestamp = formatter.string(from: Date())

        // Add timestamp to notification body
        let bodyWithTimestamp = "\(timestamp)\n\(body)"

        let content = UNMutableNotificationContent()
        content.title = title
        content.body = bodyWithTimestamp
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

        // Store notification in history
        Task { @MainActor in
            let notification = XRDPNotification(
                title: title,
                body: body,
                category: category,
                debugInfo: debugInfo
            )
            self.appState.addNotification(notification)
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
                    sound: false,
                    category: .disconnection,
                    debugInfo: "Manual disconnect by administrator\nPID: \(session.pid)"
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

struct NotificationsView: View {
    let notifications: [XRDPNotification]
    @State private var expandedNotifications: Set<UUID> = []
    @State private var selectedNotification: XRDPNotification?

    var body: some View {
        VStack(spacing: 0) {
            if notifications.isEmpty {
                VStack(spacing: 16) {
                    Image(systemName: "bell.slash")
                        .font(.system(size: 48))
                        .foregroundStyle(.secondary)
                    Text("No recent notifications")
                        .font(.title2)
                        .foregroundStyle(.secondary)
                    Text("Notifications will appear here when connections are made or errors occur")
                        .font(.caption)
                        .foregroundStyle(.tertiary)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 40)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                ScrollView {
                    LazyVStack(spacing: 1) {
                        ForEach(notifications) { notification in
                            NotificationRow(
                                notification: notification,
                                isExpanded: expandedNotifications.contains(notification.id),
                                onToggleExpand: {
                                    if expandedNotifications.contains(notification.id) {
                                        expandedNotifications.remove(notification.id)
                                    } else {
                                        expandedNotifications.insert(notification.id)
                                    }
                                },
                                onEmail: {
                                    emailNotification(notification)
                                }
                            )
                        }
                    }
                }
            }
        }
        .frame(minWidth: 800, minHeight: 600)
    }

    private func emailNotification(_ notification: XRDPNotification) {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .long

        let emailBody = """
        Notification Details:

        Title: \(notification.title)
        Category: \(notification.category.rawValue)
        Time: \(formatter.string(from: notification.timestamp))

        Description:
        \(notification.body)

        Debug Information:
        \(notification.debugInfo.isEmpty ? "No debug info available" : notification.debugInfo)

        Troubleshooting Steps:
        \(notification.troubleshootingSteps)

        ---
        System Information:
        macOS Version: \(ProcessInfo.processInfo.operatingSystemVersionString)
        XRDP Version: 0.10.0

        Please describe your issue below:


        """.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) ?? ""

        let subject = "XRDP Support: \(notification.title)".addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) ?? ""

        if let url = URL(string: "mailto:support@neutrinos.app?subject=\(subject)&body=\(emailBody)") {
            NSWorkspace.shared.open(url)
        }
    }
}

struct NotificationRow: View {
    let notification: XRDPNotification
    let isExpanded: Bool
    let onToggleExpand: () -> Void
    let onEmail: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            Button(action: onToggleExpand) {
                HStack(spacing: 12) {
                    // Category icon
                    categoryIcon
                        .font(.title2)
                        .frame(width: 30)

                    VStack(alignment: .leading, spacing: 4) {
                        Text(notification.title)
                            .font(.headline)
                            .foregroundStyle(.primary)

                        Text(timeAgo(notification.timestamp))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    Spacer()

                    Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
                        .foregroundStyle(.secondary)
                }
                .padding()
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            // Expanded content
            if isExpanded {
                VStack(alignment: .leading, spacing: 16) {
                    Divider()

                    // Timestamp
                    HStack {
                        Text("Time:")
                            .font(.caption)
                            .fontWeight(.semibold)
                        Text(fullTimestamp(notification.timestamp))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .padding(.horizontal)

                    // Category
                    HStack {
                        Text("Category:")
                            .font(.caption)
                            .fontWeight(.semibold)
                        Text(notification.category.rawValue)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .padding(.horizontal)

                    // Body
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Details:")
                            .font(.caption)
                            .fontWeight(.semibold)
                        Text(notification.body)
                            .font(.callout)
                            .foregroundStyle(.primary)
                    }
                    .padding(.horizontal)

                    // Debug info
                    if !notification.debugInfo.isEmpty {
                        VStack(alignment: .leading, spacing: 4) {
                            Text("Debug Information:")
                                .font(.caption)
                                .fontWeight(.semibold)
                            Text(notification.debugInfo)
                                .font(.system(.caption, design: .monospaced))
                                .foregroundStyle(.secondary)
                                .textSelection(.enabled)
                        }
                        .padding(.horizontal)
                        .padding(.vertical, 8)
                        .background(Color.secondary.opacity(0.1))
                        .cornerRadius(8)
                        .padding(.horizontal)
                    }

                    // Troubleshooting
                    DisclosureGroup("Troubleshooting Steps") {
                        Text(notification.troubleshootingSteps)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .textSelection(.enabled)
                            .padding(.top, 8)
                    }
                    .padding(.horizontal)

                    // Actions
                    HStack(spacing: 12) {
                        Button(action: onEmail) {
                            Label("Email to Support", systemImage: "envelope")
                        }
                        .buttonStyle(.borderedProminent)

                        Button(action: {
                            copyToClipboard()
                        }) {
                            Label("Copy Details", systemImage: "doc.on.doc")
                        }
                        .buttonStyle(.bordered)
                    }
                    .padding(.horizontal)
                    .padding(.bottom)
                }
            }
        }
        .background(Color(nsColor: .controlBackgroundColor))
        .overlay(
            Rectangle()
                .frame(height: 1)
                .foregroundStyle(Color.secondary.opacity(0.2)),
            alignment: .bottom
        )
    }

    private var categoryIcon: some View {
        switch notification.category {
        case .connection:
            return Text("✅")
        case .disconnection:
            return Text("👋")
        case .tlsError:
            return Text("🔒")
        case .protocolError:
            return Text("📡")
        case .serverCrash:
            return Text("⚠️")
        case .other:
            return Text("ℹ️")
        }
    }

    private func timeAgo(_ date: Date) -> String {
        let interval = Date().timeIntervalSince(date)
        if interval < 60 {
            return "Just now"
        } else if interval < 3600 {
            let minutes = Int(interval / 60)
            return "\(minutes) minute\(minutes == 1 ? "" : "s") ago"
        } else if interval < 86400 {
            let hours = Int(interval / 3600)
            return "\(hours) hour\(hours == 1 ? "" : "s") ago"
        } else {
            let days = Int(interval / 86400)
            return "\(days) day\(days == 1 ? "" : "s") ago"
        }
    }

    private func fullTimestamp(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "MMM d, yyyy 'at' h:mm:ss a"
        return formatter.string(from: date)
    }

    private func copyToClipboard() {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .long

        let text = """
        \(notification.title)
        Category: \(notification.category.rawValue)
        Time: \(formatter.string(from: notification.timestamp))

        \(notification.body)

        Debug Info:
        \(notification.debugInfo)
        """

        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
    }
}

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

            // Recent Notifications
            Button("Recent Notifications...") {
                showNotificationsWindow()
            }

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

    private func showNotificationsWindow() {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 800, height: 600),
            styleMask: [.titled, .closable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.center()
        window.title = "Recent Notifications"
        window.contentView = NSHostingView(rootView: NotificationsView(notifications: serverManager.appState.recentNotifications))
        window.makeKeyAndOrderFront(nil)
        window.isReleasedWhenClosed = false
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
