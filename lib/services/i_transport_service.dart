import 'dart:async';
import 'dart:typed_data';

enum DeviceConnectionState {
  disconnected,
  connecting,
  connected,
  connectionLost,
  failed,
}

abstract class ITransportService {
  Stream<DeviceConnectionState> get connectionState;
  DeviceConnectionState get currentState;
  Stream<Uint8List> get rawInputStream;

  Future<void> connect(String deviceAddress);
  Future<void> disconnect();
  Future<void> send(Uint8List bytes);
  void dispose();
}
