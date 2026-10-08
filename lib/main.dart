import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:fl_chart/fl_chart.dart';

import 'blocs/device_cubit.dart';
import 'blocs/device_state.dart';
import 'models/telemetry_model.dart';
import 'screens/verification_wizard_screen.dart';
import 'services/background_stream_parser.dart';
import 'services/command_dispatcher.dart';
import 'services/i_transport_service.dart';
import 'services/mock_telemetry_service.dart';
import 'services/resilience_manager.dart';

void main() {
  final transport = MockTelemetryService();
  final parser = BackgroundStreamParser();
  final resilience = ResilienceManager(transport);
  final dispatcher = CommandDispatcher(transport);

  runApp(
    BlocProvider(
      create: (_) => DeviceCubit(
        transport: transport,
        parser: parser,
        resilience: resilience,
        dispatcher: dispatcher,
      ),
      child: MaterialApp(
        title: 'ПН-2026',
        theme: ThemeData.dark().copyWith(
          scaffoldBackgroundColor: const Color(0xFF121212),
          cardColor: const Color(0xFF1E1E1E),
          colorScheme: const ColorScheme.dark(
            primary: Colors.lightBlueAccent,
            secondary: Colors.amberAccent,
          ),
        ),
        home: const PN2026Dashboard(),
        debugShowCheckedModeBanner: false,
      ),
    ),
  );
}

class PN2026Dashboard extends StatefulWidget {
  const PN2026Dashboard({super.key});

  @override
  State<PN2026Dashboard> createState() => _PN2026DashboardState();
}

class _PN2026DashboardState extends State<PN2026Dashboard> {
  final List<FlSpot> _ch1History = [];
  final List<FlSpot> _ch2History = [];
  final List<FlSpot> _ch3History = [];
  int _pointCounter = 0;

  void _addTelemetryPoint(TelemetryModel t) {
    _pointCounter++;
    final double x = _pointCounter.toDouble();

    setState(() {
      _ch1History.add(FlSpot(x, t.ch1.mm));
      _ch2History.add(FlSpot(x, t.ch2.mm));
      _ch3History.add(FlSpot(x, t.ch3.mm));

      if (_ch1History.length > 30) {
        _ch1History.removeAt(0);
        _ch2History.removeAt(0);
        _ch3History.removeAt(0);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ПН-2026 | Полевой наладчик'),
        actions: [
          IconButton(
            icon: const Icon(Icons.assignment_turned_in),
            tooltip: 'Поверка и катушки (Протокол)',
            onPressed: () {
              Navigator.push(
                context,
                MaterialPageRoute(
                  builder: (context) => const VerificationWizardScreen(),
                ),
              );
            },
          ),
          BlocBuilder<DeviceCubit, DeviceState>(
            builder: (context, state) {
              final isConn =
                  state.connectionState == DeviceConnectionState.connected;
              return Padding(
                padding: const EdgeInsets.only(right: 16.0),
                child: Chip(
                  avatar: CircleAvatar(
                    backgroundColor: isConn ? Colors.green : Colors.red,
                    radius: 6,
                  ),
                  label: Text(
                    isConn ? 'СВЯЗЬ OK' : 'НЕТ СВЯЗИ',
                    style: const TextStyle(
                        fontSize: 12, fontWeight: FontWeight.bold),
                  ),
                ),
              );
            },
          ),
        ],
      ),
      body: BlocListener<DeviceCubit, DeviceState>(
        listenWhen: (prev, curr) => prev.telemetry != curr.telemetry,
        listener: (context, state) {
          if (state.telemetry != null) {
            _addTelemetryPoint(state.telemetry!);
          }
        },
        child: BlocBuilder<DeviceCubit, DeviceState>(
          builder: (context, state) {
            final cubit = context.read<DeviceCubit>();
            final isConnected =
                state.connectionState == DeviceConnectionState.connected;
            final telemetry = state.telemetry;

            return SingleChildScrollView(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildControlHeader(context, cubit, isConnected, state),
                  const SizedBox(height: 16),
                  if (telemetry != null) ...[
                    Row(
                      children: [
                        Expanded(
                            child: _ChannelCard(
                                title: 'Канал 1',
                                data: telemetry.ch1,
                                color: Colors.cyanAccent)),
                        const SizedBox(width: 8),
                        Expanded(
                            child: _ChannelCard(
                                title: 'Канал 2',
                                data: telemetry.ch2,
                                color: Colors.amberAccent)),
                        const SizedBox(width: 8),
                        Expanded(
                            child: _ChannelCard(
                                title: 'Канал 3',
                                data: telemetry.ch3,
                                color: Colors.lightGreenAccent)),
                      ],
                    ),
                    const SizedBox(height: 20),
                    _buildChartCard(),
                    const SizedBox(height: 16),
                    _buildBatteryStatus(telemetry),
                  ] else
                    const Card(
                      child: Padding(
                        padding: EdgeInsets.all(32.0),
                        child: Center(
                          child: Text(
                              'Подключите эмулятор для отображения телеметрии'),
                        ),
                      ),
                    ),
                ],
              ),
            );
          },
        ),
      ),
    );
  }

  Widget _buildControlHeader(BuildContext context, DeviceCubit cubit,
      bool isConnected, DeviceState state) {
    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Row(
          children: [
            ElevatedButton.icon(
              onPressed: () {
                if (isConnected) {
                  cubit.disconnect();
                } else {
                  cubit.connect("PN2026_MOCK");
                }
              },
              icon: Icon(
                  isConnected ? Icons.power_settings_new : Icons.play_arrow),
              label: Text(isConnected ? 'Отключить' : 'Старт Эмулятора'),
              style: ElevatedButton.styleFrom(
                backgroundColor:
                    isConnected ? Colors.red.shade900 : Colors.green.shade800,
                foregroundColor: Colors.white,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: ElevatedButton.icon(
                onPressed: isConnected && !state.isSendingCommand
                    ? () => cubit.sendGeneratorCommand(
                        mode: "ACTIVE_24V", targetMa: 12.0)
                    : null,
                icon: const Icon(Icons.flash_on),
                label: Text(state.isSendingCommand
                    ? 'Тестирование...'
                    : 'Генератор 12мА'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildChartCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: const [
                Text('Осциллограмма перемещения (мм)',
                    style:
                        TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                Text('Realtime 10 Hz',
                    style: TextStyle(color: Colors.grey, fontSize: 12)),
              ],
            ),
            const SizedBox(height: 16),
            SizedBox(
              height: 200,
              child: _ch1History.isEmpty
                  ? const Center(child: Text('Ожидание данных...'))
                  : LineChart(
                      LineChartData(
                        gridData:
                            FlGridData(show: true, drawVerticalLine: false),
                        titlesData: FlTitlesData(
                          rightTitles: AxisTitles(
                              sideTitles: SideTitles(showTitles: false)),
                          topTitles: AxisTitles(
                              sideTitles: SideTitles(showTitles: false)),
                        ),
                        borderData: FlBorderData(
                            show: true,
                            border: Border.all(color: Colors.white10)),
                        lineBarsData: [
                          LineChartBarData(
                              spots: _ch1History,
                              isCurved: true,
                              color: Colors.cyanAccent,
                              barWidth: 2,
                              dotData: FlDotData(show: false)),
                          LineChartBarData(
                              spots: _ch2History,
                              isCurved: true,
                              color: Colors.amberAccent,
                              barWidth: 2,
                              dotData: FlDotData(show: false)),
                          LineChartBarData(
                              spots: _ch3History,
                              isCurved: true,
                              color: Colors.lightGreenAccent,
                              barWidth: 2,
                              dotData: FlDotData(show: false)),
                        ],
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildBatteryStatus(TelemetryModel telemetry) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16.0, vertical: 12.0),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Icon(
                  telemetry.batteryPercent > 20
                      ? Icons.battery_charging_full
                      : Icons.battery_alert,
                  color:
                      telemetry.batteryPercent > 20 ? Colors.green : Colors.red,
                ),
                const SizedBox(width: 8),
                Text(
                    'Питание ПН-2026: ${telemetry.batteryPercent}% (${telemetry.batteryVolts.toStringAsFixed(2)} В)'),
              ],
            ),
            SizedBox(
              width: 80,
              height: 8,
              child: LinearProgressIndicator(
                value: telemetry.batteryPercent / 100.0,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _ChannelCard extends StatelessWidget {
  final String title;
  final ChannelData data;
  final Color color;

  const _ChannelCard({
    required this.title,
    required this.data,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(
        side: BorderSide(color: color.withOpacity(0.5), width: 1),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Column(
          children: [
            Text(title,
                style: TextStyle(color: color, fontWeight: FontWeight.bold)),
            const Divider(height: 12),
            Text(
              '${data.mm.toStringAsFixed(2)} мм',
              style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 4),
            Text(
              '${data.ma.toStringAsFixed(2)} мА',
              style: const TextStyle(fontSize: 13, color: Colors.grey),
            ),
          ],
        ),
      ),
    );
  }
}
