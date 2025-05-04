#pragma once

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QStringList>
#include <QStringListModel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

// Include client headers
#include "client.h"
#include "peer_network.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const std::string& server_address, int p2p_port,
                      QWidget* parent = nullptr);
  ~MainWindow();

 private slots:
  // Playback control slots
  void onPlaySelected();
  void onPauseClicked();
  void onResumeClicked();
  void onStopClicked();

  // Peer connection slots
  void onRefreshPeersClicked();
  void onJoinPeerClicked();
  void onLeavePeerClicked();
  void onRefreshConnectionsClicked();
  void onGossipClicked();

  // Timer for position updates
  void onPositionTimerTimeout();

 private:
  // Setup UI components
  void setupUi();
  void setupPlaybackControls();
  void setupPeerControls();
  void setupStatusBar();

  // Load playlist from server
  void loadPlaylist();

  // Refresh connection lists
  void refreshPeerList();
  void refreshConnectionsList();

  // Client components
  std::shared_ptr<AudioClient> client_;
  std::shared_ptr<PeerNetwork> peer_network_;

  // UI components
  QTabWidget* tabWidget_;

  // Playback tab
  QWidget* playbackTab_;
  QListWidget* playlistWidget_;
  QPushButton* playButton_;
  QPushButton* pauseButton_;
  QPushButton* resumeButton_;
  QPushButton* stopButton_;
  QSlider* positionSlider_;
  QLabel* positionLabel_;
  QLabel* durationLabel_;

  // Peers tab
  QWidget* peersTab_;
  QListWidget* peerListWidget_;
  QPushButton* refreshPeersButton_;
  QPushButton* joinPeerButton_;
  QLineEdit* peerAddressEdit_;
  QListWidget* connectionsListWidget_;
  QPushButton* refreshConnectionsButton_;
  QPushButton* leavePeerButton_;
  QPushButton* gossipButton_;
  QLabel* offsetLabel_;

  // Status bar components
  QLabel* connectionStatusLabel_;
  QLabel* p2pStatusLabel_;

  // Timer for updates
  QTimer* positionTimer_;

  // Current song info
  int currentSongNum_;
  unsigned int songDuration_;
};