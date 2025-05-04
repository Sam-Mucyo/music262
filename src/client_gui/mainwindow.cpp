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
    : QMainWindow(parent), currentSongNum_(-1), songDuration_(0) {
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
  } else {
    LOG_ERROR("Failed to start P2P server on port {}", p2p_port);
    p2pStatusLabel_->setText("P2P: Failed to start");
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
    // Load playlist
    loadPlaylist();
  } else {
    connectionStatusLabel_->setText(
        QString("Server: Disconnected (%1)")
            .arg(QString::fromStdString(server_address)));
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

  // Setup status bar
  setupStatusBar();
}

void MainWindow::setupPlaybackControls() {
  QVBoxLayout* layout = new QVBoxLayout(playbackTab_);

  // Playlist
  QGroupBox* playlistGroup = new QGroupBox("Available Songs");
  QVBoxLayout* playlistLayout = new QVBoxLayout(playlistGroup);

  playlistWidget_ = new QListWidget();
  playlistLayout->addWidget(playlistWidget_);

  // Playback controls
  QGroupBox* controlsGroup = new QGroupBox("Playback Controls");
  QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);

  QHBoxLayout* buttonsLayout = new QHBoxLayout();
  playButton_ = new QPushButton("Play");
  pauseButton_ = new QPushButton("Pause");
  resumeButton_ = new QPushButton("Resume");
  stopButton_ = new QPushButton("Stop");

  buttonsLayout->addWidget(playButton_);
  buttonsLayout->addWidget(pauseButton_);
  buttonsLayout->addWidget(resumeButton_);
  buttonsLayout->addWidget(stopButton_);

  // Disable initially until a song is selected
  pauseButton_->setEnabled(false);
  resumeButton_->setEnabled(false);
  stopButton_->setEnabled(false);

  controlsLayout->addLayout(buttonsLayout);

  // Position slider and labels
  QHBoxLayout* positionLayout = new QHBoxLayout();
  positionLabel_ = new QLabel("0:00");
  positionSlider_ = new QSlider(Qt::Horizontal);
  durationLabel_ = new QLabel("0:00");

  positionLayout->addWidget(positionLabel_);
  positionLayout->addWidget(positionSlider_);
  positionLayout->addWidget(durationLabel_);

  positionSlider_->setEnabled(false);

  controlsLayout->addLayout(positionLayout);

  // Add groups to main layout
  layout->addWidget(playlistGroup, 3);
  layout->addWidget(controlsGroup, 1);

  // Connect signals and slots
  connect(playButton_, &QPushButton::clicked, this,
          &MainWindow::onPlaySelected);
  connect(pauseButton_, &QPushButton::clicked, this,
          &MainWindow::onPauseClicked);
  connect(resumeButton_, &QPushButton::clicked, this,
          &MainWindow::onResumeClicked);
  connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
}

void MainWindow::setupPeerControls() {
  QVBoxLayout* layout = new QVBoxLayout(peersTab_);

  // Peer list section
  QGroupBox* peerListGroup = new QGroupBox("Clients Connected to Server");
  QVBoxLayout* peerListLayout = new QVBoxLayout(peerListGroup);

  peerListWidget_ = new QListWidget();
  refreshPeersButton_ = new QPushButton("Refresh Peer List");

  peerListLayout->addWidget(peerListWidget_);
  peerListLayout->addWidget(refreshPeersButton_);

  // Peer connection controls
  QGroupBox* connectionGroup = new QGroupBox("Connect to Peer");
  QFormLayout* connectionLayout = new QFormLayout(connectionGroup);

  peerAddressEdit_ = new QLineEdit();
  joinPeerButton_ = new QPushButton("Join Peer");

  connectionLayout->addRow("Peer Address (ip:port):", peerAddressEdit_);
  connectionLayout->addRow(joinPeerButton_);

  // Active connections section
  QGroupBox* activeGroup = new QGroupBox("Active Peer Connections");
  QVBoxLayout* activeLayout = new QVBoxLayout(activeGroup);

  connectionsListWidget_ = new QListWidget();
  refreshConnectionsButton_ = new QPushButton("Refresh Connections");
  leavePeerButton_ = new QPushButton("Leave Selected Peer");
  gossipButton_ = new QPushButton("Gossip Connections to All Peers");
  offsetLabel_ = new QLabel("Average Offset: NA");

  activeLayout->addWidget(connectionsListWidget_);
  activeLayout->addWidget(refreshConnectionsButton_);
  activeLayout->addWidget(leavePeerButton_);
  activeLayout->addWidget(gossipButton_);
  activeLayout->addWidget(offsetLabel_);

  // Add all sections to main layout
  layout->addWidget(peerListGroup, 3);
  layout->addWidget(connectionGroup, 1);
  layout->addWidget(activeGroup, 3);

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
  setStatusBar(statusBar);

  // Add status labels
  connectionStatusLabel_ = new QLabel("Server: Disconnected");
  p2pStatusLabel_ = new QLabel("P2P: Disconnected");

  statusBar->addWidget(connectionStatusLabel_);
  statusBar->addWidget(p2pStatusLabel_);
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

void MainWindow::onPlaySelected() {
  QListWidgetItem* currentItem = playlistWidget_->currentItem();
  if (!currentItem) {
    QMessageBox::warning(this, "Error", "Please select a song to play.");
    return;
  }

  QString itemText = currentItem->text();

  // Extract song number from the item text (format: "1. Song Name")
  int songNum = itemText.section('.', 0, 0).toInt();

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

    // Play the song
    client_->Play();

    // Update button states
    playButton_->setEnabled(false);
    pauseButton_->setEnabled(true);
    resumeButton_->setEnabled(false);
    stopButton_->setEnabled(true);
  } else {
    QMessageBox::warning(this, "Error", "Failed to load the selected song.");
  }
}

void MainWindow::onPauseClicked() {
  client_->Pause();

  // Update button states
  pauseButton_->setEnabled(false);
  resumeButton_->setEnabled(true);
}

void MainWindow::onResumeClicked() {
  client_->Resume();

  // Update button states
  pauseButton_->setEnabled(true);
  resumeButton_->setEnabled(false);
}

void MainWindow::onStopClicked() {
  client_->Stop();

  // Reset UI
  positionSlider_->setValue(0);
  positionSlider_->setEnabled(false);
  positionLabel_->setText("0:00");

  // Update button states
  playButton_->setEnabled(true);
  pauseButton_->setEnabled(false);
  resumeButton_->setEnabled(false);
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