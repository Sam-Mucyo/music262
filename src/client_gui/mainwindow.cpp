#include "include/mainwindow.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <random>

#include "../client/include/audio_service_interface.h"
#include "logger.h"

MainWindow::MainWindow(const std::string& server_address, int p2p_port,
                       QWidget* parent)
    : QMainWindow(parent),
      currentSongNum_(-1),
      songDuration_(0),
      playbackState_(Stopped),
      shuffleEnabled_(false),
      repeatEnabled_(false),
      autoPlayNext_(false) {
  // Set window properties
  setWindowTitle("Music Client");
  resize(800, 600);

  // Create client
  LOG_INFO("Connecting to server at {}", server_address);
  client_ = std::make_shared<AudioClient>(
      music262::CreateAudioService(server_address));

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

  // Playback controls in a group box with a modern look
  QGroupBox* controlsGroup = new QGroupBox("Playback Controls");
  controlsGroup->setStyleSheet(
      "QGroupBox { border: 1px solid #444; border-radius: 10px; margin-top: "
      "1em; "
      "background-color: #1E1E1E; padding: 10px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 "
      "5px; "
      "color: #BB86FC; font-weight: bold; }");

  // Main layout for controls with better spacing
  QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);
  controlsLayout->setSpacing(15);
  controlsLayout->setContentsMargins(15, 20, 15, 15);

  // Create a grid layout for better organization of controls
  QGridLayout* controlsGridLayout = new QGridLayout();
  controlsGridLayout->setSpacing(10);

  // Create combined play/pause button with improved styling
  playPauseButton_ = new QPushButton("Play");
  playPauseButton_->setIcon(QIcon::fromTheme("media-playback-start"));
  playPauseButton_->setIconSize(QSize(18, 18));
  playPauseButton_->setStyleSheet(
      "QPushButton { background-color: #BB86FC; color: #121212; border-radius: "
      "20px; "
      "padding: 10px; font-weight: bold; }"
      "QPushButton:hover { background-color: #9D4EDD; }");

  stopButton_ = new QPushButton("Stop");
  stopButton_->setIcon(QIcon::fromTheme("media-playback-stop"));
  stopButton_->setIconSize(QSize(18, 18));
  stopButton_->setStyleSheet(
      "QPushButton { background-color: #BB86FC; color: #121212; border-radius: "
      "20px; "
      "padding: 10px; font-weight: bold; }"
      "QPushButton:hover { background-color: #9D4EDD; }"
      "QPushButton:disabled { background-color: #4D4D4D; color: #7F7F7F; }");

  // Create shuffle and repeat buttons with toggle styling
  shuffleButton_ = new QPushButton("Shuffle: Off");
  shuffleButton_->setIcon(QIcon::fromTheme("media-playlist-shuffle"));
  shuffleButton_->setIconSize(QSize(16, 16));
  shuffleButton_->setCheckable(true);
  shuffleButton_->setChecked(false);
  shuffleButton_->setStyleSheet(
      "QPushButton { background-color: #4D4D4D; color: #E0E0E0; border-radius: "
      "20px; "
      "padding: 8px; font-weight: bold; }"
      "QPushButton:hover { background-color: #666666; }");

  repeatButton_ = new QPushButton("Repeat: Off");
  repeatButton_->setIcon(QIcon::fromTheme("media-playlist-repeat"));
  repeatButton_->setIconSize(QSize(16, 16));
  repeatButton_->setCheckable(true);
  repeatButton_->setChecked(false);
  repeatButton_->setStyleSheet(
      "QPushButton { background-color: #4D4D4D; color: #E0E0E0; border-radius: "
      "20px; "
      "padding: 8px; font-weight: bold; }"
      "QPushButton:hover { background-color: #666666; }");

  // Set button sizes for a more balanced look
  playPauseButton_->setMinimumWidth(140);
  playPauseButton_->setMinimumHeight(45);
  stopButton_->setMinimumWidth(140);
  stopButton_->setMinimumHeight(45);
  shuffleButton_->setMinimumWidth(120);
  shuffleButton_->setMinimumHeight(40);
  repeatButton_->setMinimumWidth(120);
  repeatButton_->setMinimumHeight(40);

  // Arrange buttons in a grid for better visual organization
  // Row 0: Main playback controls
  controlsGridLayout->addWidget(playPauseButton_, 0, 0);
  controlsGridLayout->addWidget(stopButton_, 0, 1);

  // Row 1: Playlist controls
  controlsGridLayout->addWidget(shuffleButton_, 1, 0);
  controlsGridLayout->addWidget(repeatButton_, 1, 1);

  // Disable stop button initially
  stopButton_->setEnabled(false);

  // Add the grid layout to the main controls layout
  controlsLayout->addLayout(controlsGridLayout);

  // Position slider and labels with modern styling
  QWidget* sliderContainer = new QWidget();
  sliderContainer->setStyleSheet(
      "QWidget { background-color: #252525; border-radius: 8px; padding: 8px; "
      "}");
  QHBoxLayout* positionLayout = new QHBoxLayout(sliderContainer);
  positionLayout->setContentsMargins(10, 5, 10, 5);
  positionLayout->setSpacing(10);

  // Time labels with improved styling
  positionLabel_ = new QLabel("0:00");
  positionLabel_->setStyleSheet(
      "QLabel { color: #BB86FC; font-weight: bold; font-size: 13px; }");

  durationLabel_ = new QLabel("0:00");
  durationLabel_->setStyleSheet(
      "QLabel { color: #BB86FC; font-weight: bold; font-size: 13px; }");

  // Improved slider styling
  positionSlider_ = new QSlider(Qt::Horizontal);
  positionSlider_->setEnabled(false);
  positionSlider_->setMinimumWidth(300);
  positionSlider_->setStyleSheet(
      "QSlider::groove:horizontal { "
      "  border: 1px solid #444; "
      "  background: #333; "
      "  height: 8px; "
      "  border-radius: 4px; "
      "} "
      "QSlider::handle:horizontal { "
      "  background: #BB86FC; "
      "  border: 1px solid #BB86FC; "
      "  width: 16px; "
      "  margin-top: -5px; "
      "  margin-bottom: -5px; "
      "  border-radius: 8px; "
      "} "
      "QSlider::handle:horizontal:hover { "
      "  background: #9D4EDD; "
      "} "
      "QSlider::sub-page:horizontal { "
      "  background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, "
      "                            stop: 0 #BB86FC, stop: 1 #9D4EDD); "
      "  border: 1px solid #444; "
      "  height: 8px; "
      "  border-radius: 4px; "
      "}");

  positionLabel_->setMinimumWidth(45);
  durationLabel_->setMinimumWidth(45);

  positionLayout->addWidget(positionLabel_);
  positionLayout->addWidget(positionSlider_, 1);
  positionLayout->addWidget(durationLabel_);

  controlsLayout->addWidget(sliderContainer);

  // Add a spacer for better vertical alignment
  QSpacerItem* topSpacer =
      new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

  // Add components to the right panel with improved spacing
  rightLayout->addItem(topSpacer);
  rightLayout->addWidget(coverWidget, 3);
  rightLayout->addWidget(nowPlayingLabel_);
  rightLayout->addSpacing(
      15);  // Add space between now playing label and controls
  rightLayout->addWidget(controlsGroup, 1);
  rightLayout->addSpacing(10);  // Add space at the bottom
  rightLayout->setAlignment(Qt::AlignCenter);

  // Add panels to main layout
  mainLayout->addWidget(playlistGroup, 1);
  mainLayout->addWidget(rightPanel, 3);

  // Connect signals and slots
  connect(playPauseButton_, &QPushButton::clicked, this,
          &MainWindow::onPlayPauseClicked);
  connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
  connect(shuffleButton_, &QPushButton::clicked, this,
          &MainWindow::onShuffleClicked);
  connect(repeatButton_, &QPushButton::clicked, this,
          &MainWindow::onRepeatClicked);

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
  connect(positionSlider_, &QSlider::sliderPressed, this,
          [this]() { userIsSeeking_ = true; });
  connect(positionSlider_, &QSlider::sliderReleased, this,
          &MainWindow::onSliderReleased);
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

void MainWindow::updateShuffleButton() {
  if (shuffleEnabled_) {
    shuffleButton_->setText("Shuffle: On");
    shuffleButton_->setStyleSheet(
        "QPushButton { background-color: #BB86FC; color: #121212; "
        "border-radius: "
        "20px; padding: 8px; font-weight: bold; }"
        "QPushButton:hover { background-color: #9D4EDD; }");
  } else {
    shuffleButton_->setText("Shuffle: Off");
    shuffleButton_->setStyleSheet(
        "QPushButton { background-color: #4D4D4D; color: #E0E0E0; "
        "border-radius: "
        "20px; padding: 8px; font-weight: bold; }"
        "QPushButton:hover { background-color: #666666; }");
  }
}

void MainWindow::updateRepeatButton() {
  if (repeatEnabled_) {
    repeatButton_->setText("Repeat: On");
    repeatButton_->setStyleSheet(
        "QPushButton { background-color: #BB86FC; color: #121212; "
        "border-radius: "
        "20px; padding: 8px; font-weight: bold; }"
        "QPushButton:hover { background-color: #9D4EDD; }");
  } else {
    repeatButton_->setText("Repeat: Off");
    repeatButton_->setStyleSheet(
        "QPushButton { background-color: #4D4D4D; color: #E0E0E0; "
        "border-radius: "
        "20px; padding: 8px; font-weight: bold; }"
        "QPushButton:hover { background-color: #666666; }");
  }
}

void MainWindow::onShuffleClicked() {
  shuffleEnabled_ = !shuffleEnabled_;
  updateShuffleButton();

  if (shuffleEnabled_) {
    // Generate a new shuffle queue
    shuffleQueue_.clear();
    int playlistSize = playlistWidget_->count();

    // Create a vector with all song numbers
    std::vector<int> allSongs;
    for (int i = 0; i < playlistSize; i++) {
      QString itemText = playlistWidget_->item(i)->text();
      int songNum = itemText.section('.', 0, 0).toInt();
      if (songNum > 0) {  // Skip any non-song items
        allSongs.push_back(songNum);
      }
    }

    // Shuffle the vector
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allSongs.begin(), allSongs.end(), g);

    // Store in shuffle queue
    shuffleQueue_ = allSongs;

    LOG_INFO("Shuffle enabled, queue size: {}", shuffleQueue_.size());
  } else {
    LOG_INFO("Shuffle disabled");
  }
}

void MainWindow::onRepeatClicked() {
  repeatEnabled_ = !repeatEnabled_;
  updateRepeatButton();
  LOG_INFO("Repeat {}", repeatEnabled_ ? "enabled" : "disabled");
}

void MainWindow::playNextSong() {
  if (playlistWidget_->count() == 0 || playlistWidget_->count() == 1) {
    // No songs or only one song in the playlist
    if (repeatEnabled_ && currentSongNum_ > 0) {
      // If repeat is enabled, play the current song again
      if (client_->LoadAudio(currentSongNum_)) {
        client_->Play();
        playbackState_ = Playing;
        updatePlayPauseButton();
        stopButton_->setEnabled(true);
      }
    }
    return;
  }

  int nextSongNum = -1;
  QString nextSongName;

  if (shuffleEnabled_ && !shuffleQueue_.empty()) {
    // Find current song in shuffle queue
    auto it =
        std::find(shuffleQueue_.begin(), shuffleQueue_.end(), currentSongNum_);

    if (it != shuffleQueue_.end() && it + 1 != shuffleQueue_.end()) {
      // Move to next song in shuffle queue
      nextSongNum = *(it + 1);
    } else {
      // At the end of queue or current song not in queue, start from beginning
      nextSongNum = shuffleQueue_.front();
    }

    // Find the song in the playlist to get its name
    for (int i = 0; i < playlistWidget_->count(); i++) {
      QString itemText = playlistWidget_->item(i)->text();
      int songNum = itemText.section('.', 0, 0).toInt();
      if (songNum == nextSongNum) {
        nextSongName = itemText.section('.', 1).trimmed();
        playlistWidget_->setCurrentRow(i);
        break;
      }
    }
  } else if (repeatEnabled_) {
    // Find the current song in the playlist
    int currentIndex = -1;
    for (int i = 0; i < playlistWidget_->count(); i++) {
      QString itemText = playlistWidget_->item(i)->text();
      int songNum = itemText.section('.', 0, 0).toInt();
      if (songNum == currentSongNum_) {
        currentIndex = i;
        break;
      }
    }

    if (currentIndex != -1) {
      // Get the next song (or wrap around to the first song)
      int nextIndex = (currentIndex + 1) % playlistWidget_->count();
      QString itemText = playlistWidget_->item(nextIndex)->text();
      nextSongNum = itemText.section('.', 0, 0).toInt();
      nextSongName = itemText.section('.', 1).trimmed();
      playlistWidget_->setCurrentRow(nextIndex);
    }
  }

  if (nextSongNum > 0) {
    LOG_INFO("Auto-playing next song: {}", nextSongNum);

    // Load and play the next song
    if (client_->LoadAudio(nextSongNum)) {
      currentSongNum_ = nextSongNum;

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
      nowPlayingLabel_->setText(QString("Now Playing: %1").arg(nextSongName));
      nowPlayingLabel_->setStyleSheet(
          "font-size: 16px; font-weight: bold; margin-top: 10px; color: "
          "#BB86FC;");

      // Play the song
      client_->Play();

      // Update state and buttons
      playbackState_ = Playing;
      updatePlayPauseButton();
      stopButton_->setEnabled(true);
    }
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

  // Disable auto-play next
  autoPlayNext_ = false;
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

void MainWindow::onSliderReleased() {
  if (playbackState_ == Playing) {
    // If playing, briefly pause
    client_->Pause();
    playbackState_ = Paused;
    updatePlayPauseButton();
  }
  if (playbackState_ == Paused) {
    // Get the target second from the slider
    int targetSecond = positionSlider_->value();  // already in seconds

    // Update the position
    client_->SeekTo(targetSecond);

    // If paused, resume
    client_->Resume();
    playbackState_ = Playing;
    updatePlayPauseButton();
  }
  // Reset seeking to false
  userIsSeeking_ = false;
}

void MainWindow::onPositionTimerTimeout() {
  if (userIsSeeking_) {
    // If user is seeking, do not update the position
    return;
  }

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

  // Check if the command to play came from a broadcast
  if (client_->IsCommandFromBroadcast()) {
    // If broadcast command to play
    if (client_->GetBroadcastAction() == "play") {
      // Reset the controls from Pause and Stop (not greyed out but purple) to
      // Play and Stop (greyed out)
      nowPlayingLabel_->setText(QString("Now Playing"));
      nowPlayingLabel_->setStyleSheet(
          "font-size: 16px; font-weight: bold; margin-top: 10px; color: "
          "#BB86FC;");

      playbackState_ = Playing;
      updatePlayPauseButton();
      positionSlider_->setEnabled(true);
      stopButton_->setEnabled(true);
    }
    // If broadcast command to pause
    else if (client_->GetBroadcastAction() == "pause") {
      // Reset the control from Pause and Stop to Resume and Stop (both not
      // greyed out but in purple)
      nowPlayingLabel_->setText(QString("Now Paused"));
      nowPlayingLabel_->setStyleSheet(
          "font-size: 16px; font-weight: bold; margin-top: 10px; color: "
          "#BB86FC;");
      playbackState_ = Paused;
      updatePlayPauseButton();
      stopButton_->setEnabled(true);
    } else if (client_->GetBroadcastAction() == "resume") {
      // Reset the control from Pause and Stop to Resume and Stop (both not
      // greyed out but in purple)
      nowPlayingLabel_->setText(QString("Now Playing"));
      nowPlayingLabel_->setStyleSheet(
          "font-size: 16px; font-weight: bold; margin-top: 10px; color: "
          "#BB86FC;");
      playbackState_ = Playing;
      updatePlayPauseButton();
      positionSlider_->setEnabled(true);
      stopButton_->setEnabled(true);
    }
    // If broadcast command to stop
    else if (client_->GetBroadcastAction() == "stop") {
      // Reset the control Play and Stop (both not greyed out but in purple)
      // Reset UI
      positionSlider_->setValue(0);
      positionSlider_->setEnabled(false);
      positionLabel_->setText("0:00");
      nowPlayingLabel_->setText("Not Playing");
      nowPlayingLabel_->setStyleSheet(
          "font-size: 16px; font-weight: bold; margin-top: 10px;");

      playbackState_ = Stopped;
      updatePlayPauseButton();
      stopButton_->setEnabled(false);

    } else {
      // Unknown command
      LOG_ERROR("Unknown command from broadcast: {}",
                client_->GetBroadcastAction());
    }
    client_->SetCommandFromBroadcast(false);
    client_->SetCommandFromBroadcastAction(" ");
  }

  // Periodically update offset label if on peers tab
  if (tabWidget_->currentIndex() == 1) {
    float average_offset = peer_network_->GetAverageOffset();
    if (average_offset > 0) {
      offsetLabel_->setText(
          QString("Average Offset: %1 ns").arg(average_offset));
    }
  }

  // Check if song has ended and we need to play the next one
  if (playbackState_ == Playing && !client_->GetPlayer().isPlaying() &&
      (repeatEnabled_ || shuffleEnabled_)) {
    // Song has finished playing and auto-play is enabled
    autoPlayNext_ = true;
    playNextSong();
  }
}
