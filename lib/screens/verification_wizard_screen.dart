import 'package:flutter/material.dart';
import '../models/verification_model.dart';

class VerificationWizardScreen extends StatefulWidget {
  const VerificationWizardScreen({super.key});

  @override
  State<VerificationWizardScreen> createState() =>
      _VerificationWizardScreenState();
}

class _VerificationWizardScreenState extends State<VerificationWizardScreen> {
  int _currentStep = 0;

  // Конфигурация поверки для канала (напр., X / ДВТ-10)
  final SensorVerificationConfig _config = SensorVerificationConfig(
    serialNumber: 'SN-2026-0842',
    sMin: 0.0,
    sMax: 10.0,
    accuracyClass: 0.5,
    pointsCount: 5,
  );

  bool _isMeasuringCoils = false;
  int _activePointIndex = 0;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Поверка датчика (Протокол ПН-2026)'),
      ),
      body: Stepper(
        type: StepperType.vertical,
        currentStep: _currentStep,
        onStepContinue: () {
          if (_currentStep < 3) {
            setState(() => _currentStep++);
          }
        },
        onStepCancel: () {
          if (_currentStep > 0) {
            setState(() => _currentStep--);
          }
        },
        steps: [
          // ЭТАП 1: Идентификация и Параметризация
          Step(
            title: const Text('1. Идентификация и Параметризация'),
            subtitle: Text(
                'S/N: ${_config.serialNumber} | Шкала: ${_config.scale == CurrentScale.ma4_20 ? "4-20 мА" : "1-5 мА"}'),
            isActive: _currentStep >= 0,
            content: _buildStep1Parametrization(),
          ),

          // ЭТАП 2: Входной контроль катушек (Операция 2)
          Step(
            title: const Text('2. Контроль катушек (РС7ТВ)'),
            subtitle: Text(_config.coilStatus == InspectionStatus.ok
                ? 'НОРМА (Wp=${_config.rPrimary} Ом, Wc=${_config.rSecondary} Ом)'
                : _config.coilStatus == InspectionStatus.error
                    ? 'БРАК (Несоответствие R)'
                    : 'Не выполнено'),
            isActive: _currentStep >= 1,
            content: _buildStep2CoilInspection(),
          ),

          // ЭТАП 3: Пошаговый метрологический прогон (Операция 1)
          Step(
            title: const Text('3. Метрологический прогон'),
            subtitle: Text(
                'Снято точек: ${_config.points.length} из ${_config.pointsCount}'),
            isActive: _currentStep >= 2,
            content: _buildStep3MetrologyRun(),
          ),

          // ЭТАП 4: Протокол и Экспорт
          Step(
            title: const Text('4. Формирование и экспорт отчета'),
            subtitle: const Text('Фиксация во Flash / PDF / Telegram'),
            isActive: _currentStep >= 3,
            content: _buildStep4ReportExport(),
          ),
        ],
      ),
    );
  }

  // --- ЭТАП 1: Ввод серийника и параметров ---
  Widget _buildStep1Parametrization() {
    return Column(
      children: [
        TextField(
          decoration: const InputDecoration(
              labelText: 'Заводской / Серийный номер (S/N)'),
          controller: TextEditingController(text: _config.serialNumber),
          onChanged: (val) => _config.serialNumber = val,
        ),
        const SizedBox(height: 12),
        Row(
          children: [
            Expanded(
              child: DropdownButtonFormField<CurrentScale>(
                value: _config.scale,
                decoration: const InputDecoration(labelText: 'Шкала тока'),
                items: const [
                  DropdownMenuItem(
                      value: CurrentScale.ma4_20, child: Text('4 – 20 мА')),
                  DropdownMenuItem(
                      value: CurrentScale.ma1_5, child: Text('1 – 5 мА')),
                ],
                onChanged: (v) => setState(() => _config.scale = v!),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: DropdownButtonFormField<int>(
                value: _config.pointsCount,
                decoration: const InputDecoration(labelText: 'Точек поверки'),
                items: const [
                  DropdownMenuItem(
                      value: 3, child: Text('3 точки (0, 50, 100%)')),
                  DropdownMenuItem(
                      value: 5, child: Text('5 точек (0,25,50,75,100%)')),
                ],
                onChanged: (v) => setState(() => _config.pointsCount = v!),
              ),
            ),
          ],
        ),
        const SizedBox(height: 12),
        Row(
          children: [
            Expanded(
              child: TextField(
                decoration: const InputDecoration(labelText: 'Smin (мм)'),
                keyboardType: TextInputType.number,
                controller:
                    TextEditingController(text: _config.sMin.toString()),
                onChanged: (v) => _config.sMin = double.tryParse(v) ?? 0.0,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: TextField(
                decoration: const InputDecoration(labelText: 'Smax (мм)'),
                keyboardType: TextInputType.number,
                controller:
                    TextEditingController(text: _config.sMax.toString()),
                onChanged: (v) => _config.sMax = double.tryParse(v) ?? 10.0,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: TextField(
                decoration:
                    const InputDecoration(labelText: 'Класс точности (±γ%)'),
                keyboardType: TextInputType.number,
                controller: TextEditingController(
                    text: _config.accuracyClass.toString()),
                onChanged: (v) =>
                    _config.accuracyClass = double.tryParse(v) ?? 0.5,
              ),
            ),
          ],
        ),
      ],
    );
  }

  // --- ЭТАП 2: Замер сопротивлений обмоток (Операция 2) ---
  Widget _buildStep2CoilInspection() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          'Замер обмоток через разъем РС7ТВ и аналоговый мультиплексор CD74HC4051:',
          style: TextStyle(color: Colors.grey),
        ),
        const SizedBox(height: 12),
        Card(
          color: Colors.black26,
          child: Padding(
            padding: const EdgeInsets.all(12.0),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: [
                Column(
                  children: [
                    const Text('Первичная Wp (R1)',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 4),
                    Text(_config.rPrimary != null
                        ? '${_config.rPrimary!.toStringAsFixed(1)} Ом'
                        : '---'),
                  ],
                ),
                Column(
                  children: [
                    const Text('Сигнальная Wc (R2)',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 4),
                    Text(_config.rSecondary != null
                        ? '${_config.rSecondary!.toStringAsFixed(1)} Ом'
                        : '---'),
                  ],
                ),
                Column(
                  children: [
                    const Text('Статус',
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 4),
                    Chip(
                      label: Text(
                        _config.coilStatus == InspectionStatus.ok
                            ? 'НОРМА'
                            : _config.coilStatus == InspectionStatus.error
                                ? 'БРАК'
                                : 'ОЖИДАНИЕ',
                        style: const TextStyle(
                            fontWeight: FontWeight.bold, fontSize: 11),
                      ),
                      backgroundColor: _config.coilStatus == InspectionStatus.ok
                          ? Colors.green.shade800
                          : _config.coilStatus == InspectionStatus.error
                              ? Colors.red.shade800
                              : Colors.grey.shade800,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 12),
        ElevatedButton.icon(
          onPressed: _isMeasuringCoils
              ? null
              : () async {
                  setState(() => _isMeasuringCoils = true);
                  // Эмуляция замера R1 и R2 мультиплексором CD74HC4051
                  await Future.delayed(const Duration(seconds: 1));
                  setState(() {
                    _config.rPrimary = 124.5; // Норма для Wp ДВТ
                    _config.rSecondary = 125.1; // Норма для Wc ДВТ
                    _config.coilStatus = InspectionStatus.ok;
                    _isMeasuringCoils = false;
                  });
                },
          icon: _isMeasuringCoils
              ? const SizedBox(
                  width: 16,
                  height: 16,
                  child: CircularProgressIndicator(strokeWidth: 2))
              : const Icon(Icons.speed),
          label: Text(_isMeasuringCoils
              ? 'Измерение сопротивлений...'
              : 'Запустить замер катушек'),
        ),
      ],
    );
  }

  // --- ЭТАП 3: Пошаговый метрологический прогон (Операция 1) ---
  Widget _buildStep3MetrologyRun() {
    final percentages = _config.pointsCount == 5
        ? [0.0, 25.0, 50.0, 75.0, 100.0]
        : [0.0, 50.0, 100.0];
    final currentPercent = percentages[_activePointIndex % percentages.length];

    // Вычисление эталонных значений
    final range = _config.sMax - _config.sMin;
    final targetMm = _config.sMin + (range * (currentPercent / 100.0));
    final minMa = _config.scale == CurrentScale.ma4_20 ? 4.0 : 1.0;
    final maxMa = _config.scale == CurrentScale.ma4_20 ? 20.0 : 5.0;
    final targetMa = minMa + ((maxMa - minMa) * (currentPercent / 100.0));

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Card(
          color: Colors.blueGrey.shade900,
          child: Padding(
            padding: const EdgeInsets.all(12.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('Текущая контрольная точка: $currentPercent%',
                    style: const TextStyle(
                        fontWeight: FontWeight.bold, color: Colors.cyanAccent)),
                const SizedBox(height: 6),
                Text(
                    'Заданный ход: ${targetMm.toStringAsFixed(2)} мм | Расчетный ток: ${targetMa.toStringAsFixed(2)} мА'),
              ],
            ),
          ),
        ),
        const SizedBox(height: 12),
        ElevatedButton.icon(
          onPressed: () {
            // Имитация снятия показания с АЦП
            final measuredMa = targetMa + 0.03; // Небольшое отклонение
            final calcMm =
                _config.sMin + ((measuredMa - minMa) / (maxMa - minMa)) * range;
            final error = ((calcMm - targetMm) / range).abs() * 100.0;
            final isPass = error <= _config.accuracyClass;

            setState(() {
              _config.points.add(MetrologyPointResult(
                nominalPercent: currentPercent,
                targetMm: targetMm,
                measuredMa: measuredMa,
                calculatedMm: calcMm,
                errorPercent: error,
                isPass: isPass,
              ));
              if (_activePointIndex < percentages.length - 1) {
                _activePointIndex++;
              }
            });
          },
          icon: const Icon(Icons.check_circle_outline),
          label: Text('Зафиксировать точку ($currentPercent%)'),
        ),
        const SizedBox(height: 12),

        // Таблица результатов прогона
        if (_config.points.isNotEmpty)
          DataTable(
            columnSpacing: 16,
            columns: const [
              DataColumn(label: Text('%')),
              DataColumn(label: Text('Ток, мА')),
              DataColumn(label: Text('Вычисл, мм')),
              DataColumn(label: Text('γ, %')),
              DataColumn(label: Text('Статус')),
            ],
            rows: _config.points.map((p) {
              return DataRow(cells: [
                DataCell(Text('${p.nominalPercent.toInt()}%')),
                DataCell(Text(p.measuredMa.toStringAsFixed(2))),
                DataCell(Text(p.calculatedMm.toStringAsFixed(2))),
                DataCell(Text('${p.errorPercent.toStringAsFixed(2)}%')),
                DataCell(
                  Text(
                    p.isPass ? 'OK' : 'ERR',
                    style: TextStyle(
                      fontWeight: FontWeight.bold,
                      color: p.isPass ? Colors.green : Colors.red,
                    ),
                  ),
                ),
              ]);
            }).toList(),
          ),
      ],
    );
  }

  // --- ЭТАП 4: Протокол и Экспорт ---
  Widget _buildStep4ReportExport() {
    final allPassed = _config.coilStatus == InspectionStatus.ok &&
        _config.points.isNotEmpty &&
        _config.points.every((p) => p.isPass);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: allPassed
                ? Colors.green.shade900.withOpacity(0.4)
                : Colors.red.shade900.withOpacity(0.4),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: allPassed ? Colors.green : Colors.red),
          ),
          child: Row(
            children: [
              Icon(allPassed ? Icons.verified : Icons.warning,
                  color: allPassed ? Colors.green : Colors.red, size: 32),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      allPassed
                          ? 'ГОДЕН К ЭКСПЛУАТАЦИИ'
                          : 'ТРЕБУЕТСЯ КАЛИБРОВКА / БРАК',
                      style: TextStyle(
                          fontWeight: FontWeight.bold,
                          color: allPassed ? Colors.green : Colors.red),
                    ),
                    Text('Прибор: ПН-2026 | S/N: ${_config.serialNumber}',
                        style: const TextStyle(fontSize: 12)),
                  ],
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        Row(
          children: [
            Expanded(
              child: ElevatedButton.icon(
                onPressed: () {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                        content: Text(
                            'Протокол сохранен во Flash-память ПН-2026 и сгенерирован PDF!')),
                  );
                },
                icon: const Icon(Icons.picture_as_pdf),
                label: const Text('Сформировать PDF'),
              ),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: ElevatedButton.icon(
                onPressed: () {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                        content: Text('Отчет отправлен в Telegram!')),
                  );
                },
                icon: const Icon(Icons.send),
                label: const Text('В Telegram'),
                style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blue.shade700),
              ),
            ),
          ],
        ),
      ],
    );
  }
}
