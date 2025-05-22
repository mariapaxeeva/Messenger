// Конвертер для фона сообщения
// Если сообщение в чате принадлежит текущему пользователю, конвертер возвращает светло-розовую кисть, иначе - белую

using Avalonia.Data.Converters;
using Avalonia.Media;
using System;
using System.Globalization;

namespace DigiTalk.Converters
{
    public class MessageBackgroundConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool isOwnMessage)
            {
                return isOwnMessage ? new SolidColorBrush(Color.Parse("#e5ccff")) : new SolidColorBrush(Color.Parse("#ffe3fe"));
            }
            return Brushes.White;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}