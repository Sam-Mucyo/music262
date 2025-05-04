#include "include/mainwindow.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <cmath>

#include "logger.h"

MainWindow::MainWindow(const std::string& server_address, int p2p_port,
                       QWidget* parent)
    : QMainWindow(parent),
      currentSongNum_(-1),
      songDuration_(0),
      playbackState_(Stopped) {
  // Set window properties
  setWindowTitle("Music Client");
  resize(800, 600);

  // Create client
  LOG_INFO("Connecting to server at {}", server_address);
  client_ = std::make_shared<AudioClient>(
      grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

  // Set up the UI
  setupUi();

  // Create peer network
  peer_network_ = std::make_shared<PeerNetwork>(client_.get());
  client_->SetPeerNetwork(peer_network_);

  // Start P2P server
  bool p2p_started = peer_network_->StartServer(p2p_port);
  if (p2p_started) {
    LOG_INFO("P2P server started on port {}", p2p_port);
    p2pStatusLabel_->setText(QString("P2P: Connected (Port %1)").arg(p2p_port));
    p2pStatusLabel_->setStyleSheet(
        "QLabel { font-weight: bold; color: #4ecca3; }");
  } else {
    LOG_ERROR("Failed to start P2P server on port {}", p2p_port);
    p2pStatusLabel_->setText("P2P: Failed to start");
    p2pStatusLabel_->setStyleSheet(
        "QLabel { font-weight: bold; color: #f45b69; }");
    QMessageBox::warning(this, "P2P Server Error",
                         QString("Failed to start P2P server on port %1. Some "
                                 "functionality may be limited.")
                             .arg(p2p_port));
  }

  // Enable peer sync by default
  client_->EnablePeerSync(true);

  // Verify server connection
  if (client_->IsServerConnected()) {
    connectionStatusLabel_->setText(
        QString("Server: Connected (%1)")
            .arg(QString::fromStdString(server_address)));
    connectionStatusLabel_->setStyleSheet(
        "QLabel { border-right: 1px solid #444; font-weight: bold; color: "
        "#4ecca3; }");
    // Load playlist
    loadPlaylist();
  } else {
    connectionStatusLabel_->setText(
        QString("Server: Disconnected (%1)")
            .arg(QString::fromStdString(server_address)));
    connectionStatusLabel_->setStyleSheet(
        "QLabel { border-right: 1px solid #444; font-weight: bold; color: "
        "#f45b69; }");
    QMessageBox::warning(this, "Connection Error",
                         QString("Failed to connect to server at %1. Some "
                                 "functionality may be limited.")
                             .arg(QString::fromStdString(server_address)));
  }

  // Start position update timer
  positionTimer_ = new QTimer(this);
  connect(positionTimer_, &QTimer::timeout, this,
          &MainWindow::onPositionTimerTimeout);
  positionTimer_->start(100);  // Update every 100ms
}

MainWindow::~MainWindow() {
  // Stop the timer
  if (positionTimer_) {
    positionTimer_->stop();
  }

  // Stop any playback
  if (client_) {
    client_->Stop();
  }
}

void MainWindow::setupUi() {
  // Create central widget and main layout
  QWidget* centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  // Set application style with dark theme
  setWindowTitle("Music Player");
  setStyleSheet(
      // Main window background
      "QMainWindow { background-color: #121212; }"
      "QWidget { background-color: #121212; color: #E0E0E0; }"

      // Group boxes
      "QGroupBox { border: 1px solid #333; border-radius: 8px; margin-top: "
      "0.5em; background-color: #1E1E1E; color: #E0E0E0; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 "
      "3px; color: #BB86FC; }"

      // Push buttons
      "QPushButton { background-color: #BB86FC; color: #121212; border-radius: "
      "4px; padding: 8px; font-weight: bold; }"
      "QPushButton:hover { background-color: #9D4EDD; }"
      "QPushButton:disabled { background-color: #4D4D4D; color: #7F7F7F; }"

      // Slider
      "QSlider::groove:horizontal { border: 1px solid #444; background: #333; "
      "height: 6px; border-radius: 3px; }"
      "QSlider::handle:horizontal { background: #BB86FC; border: 1px solid "
      "#BB86FC; width: 12px; margin-top: -3px; margin-bottom: -3px; "
      "border-radius: 6px; }"

      // Tab widget
      "QTabWidget::pane { border: 1px solid #333; border-radius: 4px; }"
      "QTabBar::tab { background-color: #1E1E1E; padding: 8px 12px; "
      "margin-right: 2px; border-top-left-radius: 4px; "
      "border-top-right-radius: 4px; color: #E0E0E0; }"
      "QTabBar::tab:selected { background-color: #2D2D2D; border-bottom: 2px "
      "solid #BB86FC; }"

      // List widget
      "QListWidget { background-color: #2D2D2D; border-radius: 4px; border: "
      "1px "
      "solid #444; color: #E0E0E0; }"
      "QListWidget::item { padding: 5px; }"
      "QListWidget::item:selected { background-color: #3700B3; color: #FFFFFF; "
      "}"
      "QListWidget::item:hover { background-color: #333333; }"

      // Labels
      "QLabel { color: #E0E0E0; }"

      // Line edit
      "QLineEdit { background-color: #2D2D2D; border: 1px solid #444; "
      "border-radius: 4px; "
      "padding: 5px; color: #E0E0E0; }"
      "QLineEdit:focus { border: 1px solid #BB86FC; }"

      // Scrollbar
      "QScrollBar:vertical { background: #1E1E1E; width: 10px; margin: 0; }"
      "QScrollBar::handle:vertical { background: #444; min-height: 20px; "
      "border-radius: 5px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
      "background: none; }"
      "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
      "background: none; }"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
      "background: none; }");

  // Create tab widget
  tabWidget_ = new QTabWidget(centralWidget);

  // Create tabs
  playbackTab_ = new QWidget();
  peersTab_ = new QWidget();

  // Setup tabs
  setupPlaybackControls();
  setupPeerControls();

  // Add tabs to tab widget
  tabWidget_->addTab(playbackTab_, "Playback");
  tabWidget_->addTab(peersTab_, "Peer Network");

  // Main layout
  QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->addWidget(tabWidget_);
  mainLayout->setContentsMargins(12, 12, 12, 12);

  // Setup status bar
  setupStatusBar();
}

void MainWindow::setupPlaybackControls() {
  // Main horizontal layout for playback tab
  QHBoxLayout* mainLayout = new QHBoxLayout(playbackTab_);

  // LEFT SIDE: Playlist in a sidebar
  QGroupBox* playlistGroup = new QGroupBox("Available Songs");
  QVBoxLayout* playlistLayout = new QVBoxLayout(playlistGroup);

  playlistWidget_ = new QListWidget();
  playlistLayout->addWidget(playlistWidget_);

  // Set sidebar width
  playlistGroup->setMaximumWidth(250);
  playlistGroup->setMinimumWidth(200);

  // RIGHT SIDE: Cover art and controls
  QWidget* rightPanel = new QWidget();
  QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);

  // Cover art container
  QWidget* coverWidget = new QWidget();
  QVBoxLayout* coverLayout = new QVBoxLayout(coverWidget);

  // Load and display the cover art
  QLabel* coverArtLabel = new QLabel();
  // Use relative path for assets - first look in the executable's directory
  QPixmap coverArt("../src/client_gui/assets/cover.png");

  if (!coverArt.isNull()) {
    // Scale the cover art while preserving aspect ratio
    coverArt = coverArt.scaled(400, 400, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    coverArtLabel->setPixmap(coverArt);
    coverArtLabel->setAlignment(Qt::AlignCenter);

    // Add shadow effect to the cover art
    coverArtLabel->setStyleSheet(
        "border: 1px solid #444; border-radius: 8px; padding: 8px; "
        "background-color: #2D2D2D;");
  } else {
    // Fallback if image cannot be loaded
    coverArtLabel->setText("Cover Art Not Available");
    coverArtLabel->setAlignment(Qt::AlignCenter);
    coverArtLabel->setStyleSheet(
        "font-size: 16px; color: #BB86FC; border: 1px dashed #444; "
        "border-radius: "
        "8px; padding: 150px; background-color: #1E1E1E;");
  }

  coverLayout->addWidget(coverArtLabel);
  coverLayout->setAlignment(Qt::AlignCenter);

  // Add now playing info
  nowPlayingLabel_ = new QLabel("Not Playing");
  nowPlayingLabel_->setAlignment(Qt::AlignCenter);
  nowPlayingLabel_->setStyleSheet(
      "font-size: 16px; font-weight: bold; margin-top: 10px; color: #E0E0E0;");

  // Playback controls in a group box
  QGroupBox* controlsGroup = new QGroupBox("Playback Controls");
  QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);

  // Buttons in a horizontal layout with better spacing
  QHBoxLayout* buttonsLayout = new QHBoxLayout();
  buttonsLayout->setSpacing(10);
  buttonsLayout->setContentsMargins(10, 10, 10, 10);

  // Create combined play/pause button
  playPauseButton_ = new QPushButton("Play");
  playPauseButton_->setIcon(QIcon::fromTheme("media-playback-start"));

  stopButton_ = new QPushButton("Stop");
  stopButton_->setIcon(QIcon::fromTheme("media-playback-stop"));

  // Set button sizes
  playPauseButton_->setMinimumWidth(120);
  playPauseButton_->setMinimumHeight(40);
  stopButton_->setMinimumWidth(120);
  stopButton_->setMinimumHeight(40);

  // Add buttons to layout
  buttonsLayout->addWidget(playPauseButton_);
  buttonsLayout->addWidget(stopButton_);
  buttonsLayout->setAlignment(Qt::AlignCenter);

  // Disable stop button initially
  stopButton_->setEnabled(false);

  controlsLayout->addLayout(buttonsLayout);

  // Position slider and labels with better styling
  QHBoxLayout* positionLayout = new QHBoxLayout();
  positionLabel_ = new QLabel("0:00");
  positionSlider_ = new QSlider(Qt::Horizontal);
  durationLabel_ = new QLabel("0:00");

  positionSlider_->setEnabled(false);
  positionSlider_->setMinimumWidth(300);
  positionLabel_->setMinimumWidth(40);
  durationLabel_->setMinimumWidth(40);

  positionLayout->addWidget(positionLabel_);
  positionLayout->addWidget(positionSlider_);
  positionLayout->addWidget(durationLabel_);

  controlsLayout->addLayout(positionLayout);

  // Add components to the right panel
  rightLayout->addWidget(coverWidget, 3);
  rightLayout->addWidget(nowPlayingLabel_);
  rightLayout->addWidget(controlsGroup, 1);
  rightLayout->setAlignment(Qt::AlignCenter);

  // Add panels to main layout
  mainLayout->addWidget(playlistGroup, 1);
  mainLayout->addWidget(rightPanel, 3);

  // Connect signals and slots
  connect(playPauseButton_, &QPushButton::clicked, this,
          &MainWindow::onPlayPauseClicked);
  connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);

  // Double-click on a playlist item to play it immediately
  connect(playlistWidget_, &QListWidget::itemDoubleClicked,
          [=](QListWidgetItem*) { playSelectedSong(); });
}

void MainWindow::setupPeerControls() {
  // Main horizontal layout for peer tab
  QHBoxLayout* mainLayout = new QHBoxLayout(peersTab_);

  // LEFT SIDE: Peer lists and connection status
  QWidget* leftPanel = new QWidget();
  QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);

  // Peer list section
  QGroupBox* peerListGroup = new QGroupBox("Clients Connected to Server");
  QVBoxLayout* peerListLayout = new QVBoxLayout(peerListGroup);

  peerListWidget_ = new QListWidget();
  peerListWidget_->setStyleSheet(
      "QListWidget { min-height: 150px; background-color: #2D2D2D; }");
  refreshPeersButton_ = new QPushButton("Refresh Peer List");
  refreshPeersButton_->setIcon(QIcon::fromTheme("view-refresh"));

  peerListLayout->addWidget(peerListWidget_);
  peerListLayout->addWidget(refreshPeersButton_);

  // Active connections section
  QGroupBox* activeGroup = new QGroupBox("Active Peer Connections");
  QVBoxLayout* activeLayout = new QVBoxLayout(activeGroup);

  connectionsListWidget_ = new QListWidget();
  connectionsListWidget_->setStyleSheet(
      "QListWidget { min-height: 150px; background-color: #2D2D2D; }");

  // Organize buttons in a more compact horizontal layout
  QHBoxLayout* connectionButtonsLayout = new QHBoxLayout();

  refreshConnectionsButton_ = new QPushButton("Refresh");
  leavePeerButton_ = new QPushButton("Leave Selected Peer");

  connectionButtonsLayout->addWidget(refreshConnectionsButton_);
  connectionButtonsLayout->addWidget(leavePeerButton_);

  // Add connection status with better styling
  offsetLabel_ = new QLabel("Average Offset: NA");
  offsetLabel_->setStyleSheet(
      "font-weight: bold; color: #BB86FC; margin-top: 10px;");
  offsetLabel_->setAlignment(Qt::AlignCenter);

  activeLayout->addWidget(connectionsListWidget_);
  activeLayout->addLayout(connectionButtonsLayout);
  activeLayout->addWidget(offsetLabel_);

  // Add both groups to left panel
  leftLayout->addWidget(peerListGroup);
  leftLayout->addWidget(activeGroup);

  // RIGHT SIDE: Connection controls and gossip section
  QWidget* rightPanel = new QWidget();
  QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);

  // Peer connection controls in a card-like style
  QGroupBox* connectionGroup = new QGroupBox("Connect to Peer");
  QVBoxLayout* connectionLayout = new QVBoxLayout(connectionGroup);

  QLabel* addressLabel = new QLabel("Peer Address (ip:port):");
  addressLabel->setStyleSheet("color: #E0E0E0; font-weight: bold;");

  peerAddressEdit_ = new QLineEdit();
  peerAddressEdit_->setPlaceholderText("e.g., 192.168.1.100:50052");
  peerAddressEdit_->setStyleSheet(
      "padding: 8px; border: 1px solid #444; border-radius: 4px; "
      "background-color: #2D2D2D; color: #E0E0E0;");

  joinPeerButton_ = new QPushButton("Join Peer");
  joinPeerButton_->setMinimumHeight(36);

  connectionLayout->addWidget(addressLabel);
  connectionLayout->addWidget(peerAddressEdit_);
  connectionLayout->addWidget(joinPeerButton_);
  connectionLayout->setContentsMargins(20, 20, 20, 20);

  // Gossip section with visual emphasis
  QGroupBox* gossipGroup = new QGroupBox("Network Propagation");
  QVBoxLayout* gossipLayout = new QVBoxLayout(gossipGroup);

  QLabel* gossipInfo = new QLabel(
      "Share your peer connections with all connected peers to help build a "
      "more robust network.");
  gossipInfo->setWordWrap(true);
  gossipInfo->setStyleSheet("color: #E0E0E0; margin-bottom: 10px;");

  gossipButton_ = new QPushButton("Gossip Connections to All Peers");
  gossipButton_->setMinimumHeight(36);

  gossipLayout->addWidget(gossipInfo);
  gossipLayout->addWidget(gossipButton_);
  gossipLayout->setContentsMargins(20, 20, 20, 20);

  // Add connection and gossip to right panel
  rightLayout->addWidget(connectionGroup);
  rightLayout->addWidget(gossipGroup);
  rightLayout->addStretch(1);  // Push everything to the top

  // Set widths for the panels
  leftPanel->setMinimumWidth(400);
  rightPanel->setMinimumWidth(300);

  // Add both panels to main layout
  mainLayout->addWidget(leftPanel, 3);
  mainLayout->addWidget(rightPanel, 2);

  // Connect signals and slots
  connect(refreshPeersButton_, &QPushButton::clicked, this,
          &MainWindow::onRefreshPeersClicked);
  connect(joinPeerButton_, &QPushButton::clicked, this,
          &MainWindow::onJoinPeerClicked);
  connect(refreshConnectionsButton_, &QPushButton::clicked, this,
          &MainWindow::onRefreshConnectionsClicked);
  connect(leavePeerButton_, &QPushButton::clicked, this,
          &MainWindow::onLeavePeerClicked);
  connect(gossipButton_, &QPushButton::clicked, this,
          &MainWindow::onGossipClicked);
}

void MainWindow::setupStatusBar() {
  // Create status bar
  QStatusBar* statusBar = new QStatusBar(this);
  statusBar->setStyleSheet(
      "QStatusBar { background-color: #1A1A1A; border-top: 1px solid #333; }"
      "QLabel { padding: 4px 10px; color: #E0E0E0; }");
  setStatusBar(statusBar);

  // Add status labels with icons and better styling
  connectionStatusLabel_ = new QLabel("Server: Disconnected");
  connectionStatusLabel_->setStyleSheet(
      "QLabel { border-right: 1px solid #444; font-weight: bold; color: "
      "#E0E0E0; }");

  p2pStatusLabel_ = new QLabel("P2P: Disconnected");
  p2pStatusLabel_->setStyleSheet("font-weight: bold; color: #E0E0E0;");

  // Create a spacer to push the connection info to the right
  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  // Add version label
  QLabel* versionLabel = new QLabel("v1.0");
  versionLabel->setStyleSheet("color: #BB86FC;");

  // Add all widgets to status bar
  statusBar->addWidget(connectionStatusLabel_);
  statusBar->addWidget(p2pStatusLabel_);
  statusBar->addPermanentWidget(spacer);
  statusBar->addPermanentWidget(versionLabel);
}

void MainWindow::loadPlaylist() {
  playlistWidget_->clear();

  std::vector<std::string> playlist = client_->GetPlaylist();
  if (playlist.empty()) {
    QListWidgetItem* item =
        new QListWidgetItem("No songs available on the server");
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    playlistWidget_->addItem(item);
  } else {
    for (size_t i = 0; i < playlist.size(); i++) {
      QString item =
          QString("%1. %2").arg(i + 1).arg(QString::fromStdString(playlist[i]));
      playlistWidget_->addItem(item);
    }
  }
}

void MainWindow::refreshPeerList() {
  peerListWidget_->clear();

  std::vector<std::string> peers = client_->GetPeerClientIPs();
  if (peers.empty()) {
    QListWidgetItem* item =
        new QListWidgetItem("No other clients connected to server");
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    peerListWidget_->addItem(item);
  } else {
    for (size_t i = 0; i < peers.size(); i++) {
      QString item =
          QString("%1. %2").arg(i + 1).arg(QString::fromStdString(peers[i]));
      peerListWidget_->addItem(item);
    }
  }
}

void MainWindow::refreshConnectionsList() {
  connectionsListWidget_->clear();

  std::vector<std::string> connected_peers = peer_network_->GetConnectedPeers();
  if (connected_peers.empty()) {
    QListWidgetItem* item = new QListWidgetItem("No active peer connections");
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    connectionsListWidget_->addItem(item);
  } else {
    for (size_t i = 0; i < connected_peers.size(); i++) {
      QString item = QString("%1. %2").arg(i + 1).arg(
          QString::fromStdString(connected_peers[i]));
      connectionsListWidget_->addItem(item);
    }
  }

  float average_offset = peer_network_->GetAverageOffset();
  if (average_offset > 0) {
    offsetLabel_->setText(QString("Average Offset: %1 ns").arg(average_offset));
  } else {
    offsetLabel_->setText("Average Offset: NA");
  }
}

void MainWindow::onPlayPauseClicked() {
  if (playbackState_ == Stopped) {
    // If stopped, load and play a song
    playSelectedSong();
  } else if (playbackState_ == Playing) {
    // If playing, pause
    client_->Pause();
    playbackState_ = Paused;
    updatePlayPauseButton();
  } else if (playbackState_ == Paused) {
    // If paused, resume
    client_->Resume();
    playbackState_ = Playing;
    updatePlayPauseButton();
  }
}

void MainWindow::playSelectedSong() {
  QListWidgetItem* currentItem = playlistWidget_->currentItem();
  if (!currentItem) {
    QMessageBox::warning(this, "Error", "Please select a song to play.");
    return;
  }

  QString itemText = currentItem->text();

  // Extract song number from the item text (format: "1. Song Name")
  int songNum = itemText.section('.', 0, 0).toInt();
  // Extract song name for display
  QString songName = itemText.section('.', 1).trimmed();

  // Load and play the song
  if (client_->LoadAudio(songNum)) {
    currentSongNum_ = songNum;

    // Get song duration from audio data size and format
    const WavHeader& header = client_->GetPlayer().get_header();
    songDuration_ =
        client_->GetAudioData().size() /
        (header.numChannels * (header.bitsPerSample / 8) * header.sampleRate);

    // Update duration label (format: MM:SS)
    int minutes = songDuration_ / 60;
    int seconds = songDuration_ % 60;
    durationLabel_->setText(
        QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));

    // Update slider range
    positionSlider_->setRange(0, songDuration_);
    positionSlider_->setValue(0);
    positionSlider_->setEnabled(true);

    // Update now playing label with formatted text
    nowPlayingLabel_->setText(QString("Now Playing: %1").arg(songName));
    nowPlayingLabel_->setStyleSheet(
        "font-size: 16px; font-weight: bold; margin-top: 10px; color: "
        "#BB86FC;");

    // Play the song
    client_->Play();

    // Update state and buttons
    playbackState_ = Playing;
    updatePlayPauseButton();
    stopButton_->setEnabled(true);
  } else {
    QMessageBox::warning(this, "Error", "Failed to load the selected song.");
  }
}

void MainWindow::updatePlayPauseButton() {
  switch (playbackState_) {
    case Playing:
      playPauseButton_->setText("Pause");
      playPauseButton_->setIcon(QIcon::fromTheme("media-playback-pause"));
      break;
    case Paused:
      playPauseButton_->setText("Resume");
      playPauseButton_->setIcon(QIcon::fromTheme("media-playback-start"));
      break;
    case Stopped:
      playPauseButton_->setText("Play");
      playPauseButton_->setIcon(QIcon::fromTheme("media-playback-start"));
      break;
  }
}

void MainWindow::onStopClicked() {
  client_->Stop();

  // Reset UI
  positionSlider_->setValue(0);
  positionSlider_->setEnabled(false);
  positionLabel_->setText("0:00");
  nowPlayingLabel_->setText("Not Playing");
  nowPlayingLabel_->setStyleSheet(
      "font-size: 16px; font-weight: bold; margin-top: 10px;");

  // Update state and buttons
  playbackState_ = Stopped;
  updatePlayPauseButton();
  stopButton_->setEnabled(false);
}

void MainWindow::onRefreshPeersClicked() { refreshPeerList(); }

void MainWindow::onJoinPeerClicked() {
  QString peerAddress = peerAddressEdit_->text().trimmed();
  if (peerAddress.isEmpty()) {
    QMessageBox::warning(this, "Error",
                         "Please enter a peer address in the format ip:port.");
    return;
  }

  if (peer_network_->ConnectToPeer(peerAddress.toStdString())) {
    QMessageBox::information(this, "Success",
                             QString("Connected to peer: %1").arg(peerAddress));
    refreshConnectionsList();
  } else {
    QMessageBox::warning(
        this, "Error",
        QString("Failed to connect to peer: %1").arg(peerAddress));
  }
}

void MainWindow::onLeavePeerClicked() {
  QListWidgetItem* currentItem = connectionsListWidget_->currentItem();
  if (!currentItem || currentItem->text().startsWith("No active")) {
    QMessageBox::warning(this, "Error",
                         "Please select a peer connection to leave.");
    return;
  }

  // Extract peer address from the item text (format: "1. Peer Address")
  QString itemText = currentItem->text();
  QString peerAddress = itemText.section(' ', 1).trimmed();

  if (peer_network_->DisconnectFromPeer(peerAddress.toStdString())) {
    QMessageBox::information(
        this, "Success",
        QString("Disconnected from peer: %1").arg(peerAddress));
    refreshConnectionsList();
  } else {
    QMessageBox::warning(
        this, "Error",
        QString("Failed to disconnect from peer: %1").arg(peerAddress));
  }
}

void MainWindow::onRefreshConnectionsClicked() { refreshConnectionsList(); }

void MainWindow::onGossipClicked() {
  peer_network_->BroadcastGossip();
  QMessageBox::information(this, "Gossip",
                           "Gossiping peer connections to all peers.");
}

void MainWindow::onPositionTimerTimeout() {
  if (client_->GetPlayer().isPlaying()) {
    // Get position in bytes
    unsigned int positionBytes = client_->GetPosition();

    // Convert to seconds based on format
    const WavHeader& header = client_->GetPlayer().get_header();
    if (header.sampleRate > 0 && header.numChannels > 0 &&
        header.bitsPerSample > 0) {
      unsigned int bytesPerSample =
          (header.bitsPerSample / 8) * header.numChannels;
      unsigned int positionSamples = positionBytes / bytesPerSample;
      unsigned int positionSeconds = positionSamples / header.sampleRate;

      // Update slider and label
      positionSlider_->setValue(positionSeconds);

      // Format position as MM:SS
      int minutes = positionSeconds / 60;
      int seconds = positionSeconds % 60;
      positionLabel_->setText(
          QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));
    }
  }

  // Periodically update offset label if on peers tab
  if (tabWidget_->currentIndex() == 1) {
    float average_offset = peer_network_->GetAverageOffset();
    if (average_offset > 0) {
      offsetLabel_->setText(
          QString("Average Offset: %1 ns").arg(average_offset));
    }
  }
}
