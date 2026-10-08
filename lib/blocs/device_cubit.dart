import 'dart:async';
import 'dart:convert';

import 'package:flutter_bloc/flutter_bloc.dart';

import '../models/telemetry_model.dart';
import '../services/background_stream_parser.dart';
import '../services/command_dispatcher.dart';
import '../services/i_transport_service.dart';
import '../services/resilience_manager.dart';
import 'device_state.dart';

class DeviceCubit extends Cubit<DeviceState> {
  final ITransportService _transport;
  final BackgroundStreamParser _parser;
  final ResilienceManager _resilience;
  final CommandDispatcher _dispatcher;

  StreamSubscription? _rawSub;

  DeviceCubit({
    required ITransportService transport,
    required BackgroundStreamParser parser,
    required ResilienceManager resilience,
    required CommandDispatcher dispatcher,
  }) : _transport = transport,
       _parser = parser,
       _resilience = resilience,
       _dispatcher = dispatcher,
       super(
         const DeviceState(connectionState: DeviceConnectionState.disconnected),
       ) {
    _initStream();
  }

  void _initStream() {
    _rawSub = _transport.rawInputStream
        .transform(utf8.decoder)
        .transform(const LineSplitter())
        .listen((line) {
      _dispatcher.handleIncomingRawJson(line);

      final items = _parser.parseLine(line);
      if (items.isNotEmpty) {
        _resilience.notifyPacketReceived();
        emit(
          state.copyWith(
            telemetry: items.last,
            connectionState: DeviceConnectionState.connected,
          ),
        );
      }
    }, onError: (e) {
      // Ignore stream errors
    });
  }

  Future<void> connect(String deviceId) async {
    emit(state.copyWith(statusMessage: 'Подключение...'));
    await _transport.connect(deviceId);
    emit(
      state.copyWith(
        connectionState: DeviceConnectionState.connected,
        statusMessage: 'Подключено',
      ),
    );
  }

  Future<void> disconnect() async {
    await _transport.disconnect();
    emit(
      state.copyWith(
        connectionState: DeviceConnectionState.disconnected,
        telemetry: null,
        statusMessage: 'Отключено',
      ),
    );
  }

  Future<void> sendGeneratorCommand({
    required String mode,
    required double targetMa,
  }) async {
    emit(state.copyWith(isSendingCommand: true));
    final res = await _dispatcher.sendSetGeneratorCommand(
      mode: mode,
      targetMa: targetMa,
    );
    emit(
      state.copyWith(
        isSendingCommand: false,
        statusMessage: 'Команда: ${res.statusText} (ID: ${res.transId})',
      ),
    );
  }

  @override
  Future<void> close() {
    _rawSub?.cancel();
    _resilience.dispose();
    _transport.dispose();
    return super.close();
  }
}
