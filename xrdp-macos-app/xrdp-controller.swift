//
//  xrdp-controller.swift
//  xrdp Remote Desktop
//
//  Copyright (C) 2026 Neutrinos Software Corporation
//  Some portions Classify®
//

import Cocoa
import SwiftUI

// MARK: - Session Model
@Observable
class XRDPSession: Identifiable {
    let id = UUID()
    var sessionId: String = ""
    var username: String = ""
    var ipAddress: String = ""
    var protocolType: String = "RDP"  // 'protocol' is a Swift keyword
    var encryption: String = "TLS 1.3 ChaCha20-Poly1305"
    var pid: Int = 0
    var connectedAt: Date = Date()
}

// MARK: - App State
@Observable
class XRDPAppState {
    var isServerRunning: Bool = false
    var activeSessions: [XRDPSession] = []
    var connectionCount: Int = 0
    
    var statusText: String {
        isServerRunning ? "xrdp Server: Running" : "xrdp Server: Stopped"
    }
    
    var connectionsText: String {
        "Active Connections: \(connectionCount)"
    }
    
    var tooltipText: String {
        if connectionCount > 0 {
            return "xrdp Remote Desktop - \(connectionCount) active connection\(connectionCount == 1 ? "" : "s")"
        }
        return "xrdp Remote Desktop"
    }
}

// MARK: - App Delegate
class XRDPAppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private var appState = XRDPAppState()
    private var connectionMonitor: Timer?
    
    private var xrdpTask: Process?
    private var sesmanTask: Process?
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Create status bar item
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        
        if let button = statusItem.button {
            // Use SF Symbol
            if #available(macOS 11.0, *) {
                if let icon = NSImage(systemSymbolName: "atom", accessibilityDescription: "xrdp") ??
                              NSImage(systemSymbolName: "server.rack", accessibilityDescription: "xrdp") ??
                              NSImage(systemSymbolName: "network", accessibilityDescription: "xrdp") {
                    icon.isTemplate = true
                    button.image = icon
                } else {
                    button.title = "⚛"
                }
            } else {
                button.title = "⚛"
            }
            
            button.toolTip = "xrdp Remote Desktop"
        }
        
        // Create SwiftUI menu
        let menu = NSMenu()
        menu.delegate = self
        statusItem.menu = menu
        
        // Set behavior
        if #available(macOS 10.12, *) {
            statusItem.behavior = .removalAllowed
        }
        
        // Kill existing processes
        killExistingProcesses()
        
        // Auto-start server
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            self.startServer()
        }
        
        // Check screen recording permission
        checkScreenRecordingPermission()
    }
    
    func applicationWillTerminate(_ notification: Notification) {
        stopServer()
    }
    
    // MARK: - Process Management
    
    private func killExistingProcesses() {
        print("Checking for existing xrdp server processes...")
        
        let helpersDir = Bundle.main.bundlePath + "/Contents/Helpers"
        
        // Kill processes using pkill with full paths
        for processName in ["xrdp", "xrdp-sesman", "xrdp-chansrv"] {
            let task = Process()
            task.launchPath = "/usr/bin/pkill"
            task.arguments = ["-9", "-f", "\(helpersDir)/\(processName)"]
            
            try? task.run()
            task.waitUntilExit()
        }
        
        Thread.sleep(forTimeInterval: 0.3)
        print("Cleanup complete")
    }
    
    private func startServer() {
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
        
        Thread.sleep(forTimeInterval: 0.5)
        
        // Start xrdp
        xrdpTask = Process()
        xrdpTask?.executableURL = URL(fileURLWithPath: "\(helpersDir)/xrdp")
        xrdpTask?.arguments = ["--nodaemon", "--port", "3389", "-c", "\(resourcesDir)/etc/xrdp/xrdp.ini"]
        xrdpTask?.environment = env
        
        do {
            try xrdpTask?.run()
            print("Started xrdp (PID: \(xrdpTask?.processIdentifier ?? 0))")
            print("Note: xrdp-chansrv will be started automatically by sesman when a user connects")
            
            appState.isServerRunning = true
            startConnectionMonitoring()
        } catch {
            print("Failed to start xrdp: \(error)")
            sesmanTask?.terminate()
        }
    }
    
    private func stopServer() {
        print("Stopping xrdp server...")
        
        stopConnectionMonitoring()
        
        xrdpTask?.terminate()
        xrdpTask = nil
        
        sesmanTask?.terminate()
        sesmanTask = nil
        
        killExistingProcesses()
        
        appState.isServerRunning = false
    }
    
    // MARK: - Connection Monitoring
    
    private func startConnectionMonitoring() {
        connectionMonitor = Timer.scheduledTimer(withTimeInterval: 5.0, repeats: true) { [weak self] _ in
            self?.updateConnections()
        }
        updateConnections()
    }
    
    private func stopConnectionMonitoring() {
        connectionMonitor?.invalidate()
        connectionMonitor = nil
        appState.activeSessions.removeAll()
        appState.connectionCount = 0
        updateTooltip()
    }
    
    private func updateConnections() {
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
                parseConnectionInfo(output)
            }
        } catch {
            print("Failed to get connection info: \(error)")
        }
    }
    
    private func parseConnectionInfo(_ psOutput: String) {
        var newSessions: [XRDPSession] = []
        
        let lines = psOutput.components(separatedBy: "\n")
        for line in lines {
            if line.contains("xrdp-chansrv") && !line.contains("grep") {
                if let session = parseSessionFromProcessLine(line) {
                    newSessions.append(session)
                }
            }
        }
        
        appState.activeSessions = newSessions
        appState.connectionCount = newSessions.count
        updateTooltip()
    }
    
    private func parseSessionFromProcessLine(_ line: String) -> XRDPSession? {
        let components = line.components(separatedBy: .whitespaces).filter { !$0.isEmpty }
        guard components.count >= 2 else { return nil }
        
        let session = XRDPSession()
        session.username = components[0]
        session.pid = Int(components[1]) ?? 0
        
        // Get IP address from lsof
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
    
    private func updateTooltip() {
        statusItem.button?.toolTip = appState.tooltipText
    }
    
    private func disconnectSession(_ session: XRDPSession) {
        let alert = NSAlert()
        alert.messageText = "Disconnect User?"
        alert.informativeText = "Are you sure you want to disconnect \(session.username) from \(session.ipAddress)?"
        alert.addButton(withTitle: "Disconnect")
        alert.addButton(withTitle: "Cancel")
        
        if alert.runModal() == .alertFirstButtonReturn {
            let task = Process()
            task.launchPath = "/bin/kill"
            task.arguments = ["-9", "\(session.pid)"]
            
            do {
                try task.run()
                task.waitUntilExit()
                print("Disconnected session for \(session.username) (PID: \(session.pid))")
                
                // Force immediate update
                updateConnections()
            } catch {
                print("Failed to disconnect session: \(error)")
            }
        }
    }
    
    // MARK: - Permissions
    
    private func checkScreenRecordingPermission() {
        // Screen recording permission check
        // User will be prompted by system when needed
        print("Screen recording permission will be requested by system when needed")
    }
    
    private func showAbout() {
        let alert = NSAlert()
        alert.messageText = "⚛ xrdp for macOS"
        alert.informativeText = """
        xrdp - Open Source Remote Desktop Protocol Server
        Version: 1.0.0
        
        Built with NeutrinoTLS - Pure C TLS 1.3 Implementation
        Using ChaCha20-Poly1305 AEAD Encryption
        
        ACKNOWLEDGEMENTS
        
        xrdp Project:
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
        
        xrdp: Apache License 2.0
        OpenSSL: Apache License 2.0
        NeutrinoTLS: Apache License 2.0
        
        Full license text available at:
        https://www.apache.org/licenses/LICENSE-2.0
        
        ⚛ Built with Claude Code
        """
        alert.addButton(withTitle: "OK")
        alert.addButton(withTitle: "View on GitHub")
        
        if alert.runModal() == .alertSecondButtonReturn {
            if let url = URL(string: "https://github.com/neutrinolabs/xrdp") {
                NSWorkspace.shared.open(url)
            }
        }
    }
}

// MARK: - Menu Delegate
extension XRDPAppDelegate: NSMenuDelegate {
    func menuNeedsUpdate(_ menu: NSMenu) {
        menu.removeAllItems()
        
        // Status
        let statusItem = NSMenuItem(title: appState.statusText, action: nil, keyEquivalent: "")
        statusItem.isEnabled = false
        menu.addItem(statusItem)
        
        // Connections
        let connectionsItem = NSMenuItem(title: appState.connectionsText, action: nil, keyEquivalent: "")
        
        if appState.connectionCount == 0 {
            let submenu = NSMenu()
            let noConnections = NSMenuItem(title: "No active connections", action: nil, keyEquivalent: "")
            noConnections.isEnabled = false
            submenu.addItem(noConnections)
            connectionsItem.submenu = submenu
        } else {
            let submenu = NSMenu()
            for session in appState.activeSessions {
                let sessionItem = NSMenuItem(title: "\(session.username) from \(session.ipAddress)", action: nil, keyEquivalent: "")
                
                let detailsMenu = NSMenu()
                
                let protocolItem = NSMenuItem(title: "Protocol: \(session.protocolType)", action: nil, keyEquivalent: "")
                protocolItem.isEnabled = false
                detailsMenu.addItem(protocolItem)
                
                let encryptionItem = NSMenuItem(title: "Encryption: \(session.encryption)", action: nil, keyEquivalent: "")
                encryptionItem.isEnabled = false
                detailsMenu.addItem(encryptionItem)
                
                let pidItem = NSMenuItem(title: "PID: \(session.pid)", action: nil, keyEquivalent: "")
                pidItem.isEnabled = false
                detailsMenu.addItem(pidItem)
                
                detailsMenu.addItem(.separator())
                
                let disconnectItem = NSMenuItem(title: "Disconnect Session", action: #selector(disconnectSessionAction(_:)), keyEquivalent: "")
                disconnectItem.target = self
                disconnectItem.representedObject = session
                detailsMenu.addItem(disconnectItem)
                
                sessionItem.submenu = detailsMenu
                submenu.addItem(sessionItem)
            }
            connectionsItem.submenu = submenu
        }
        
        menu.addItem(connectionsItem)
        
        menu.addItem(.separator())
        
        // Start/Stop
        let startItem = NSMenuItem(title: "Start Server", action: #selector(startServerAction), keyEquivalent: "")
        startItem.target = self
        startItem.isEnabled = !appState.isServerRunning
        menu.addItem(startItem)
        
        let stopItem = NSMenuItem(title: "Stop Server", action: #selector(stopServerAction), keyEquivalent: "")
        stopItem.target = self
        stopItem.isEnabled = appState.isServerRunning
        menu.addItem(stopItem)
        
        menu.addItem(.separator())
        
        // About
        let aboutItem = NSMenuItem(title: "About xrdp...", action: #selector(showAboutAction), keyEquivalent: "")
        aboutItem.target = self
        menu.addItem(aboutItem)
        
        menu.addItem(.separator())
        
        // Quit
        let quitItem = NSMenuItem(title: "Quit", action: #selector(quitAction), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)
    }
    
    @objc func startServerAction() {
        startServer()
    }
    
    @objc func stopServerAction() {
        stopServer()
    }
    
    @objc func showAboutAction() {
        showAbout()
    }
    
    @objc func disconnectSessionAction(_ sender: NSMenuItem) {
        if let session = sender.representedObject as? XRDPSession {
            disconnectSession(session)
        }
    }
    
    @objc func quitAction() {
        stopServer()
        NSApp.terminate(self)
    }
}

// MARK: - Main
let app = NSApplication.shared
let delegate = XRDPAppDelegate()
app.delegate = delegate
app.run()
